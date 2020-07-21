/**
 * TODO : Redo error messages (right now they are awful)
 */

#include "util/fmt/format.h"
#include "util/fmt/ostream.h"
#include <deque>
#include <memory>
#include <ostream>
#include <stack>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/ast/ast.h"
#include "parser/ast/transform.h"
#include "parser/ast/transform_ctx.h"
#include "parser/ast/types.h"
#include "parser/common.h"
#include "parser/ocaml.h"

using fmt::format;
using std::deque;
using std::dynamic_pointer_cast;
using std::get;
using std::make_shared;
using std::make_unique;
using std::move;
using std::ostream;
using std::pair;
using std::shared_ptr;
using std::stack;
using std::static_pointer_cast;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::unordered_set;
using std::vector;

namespace seq {
namespace ast {

using namespace types;

template <typename T> string v2s(const vector<T> &targs) {
  vector<string> args;
  for (auto &t : targs)
    args.push_back(t->toString());
  return join(args, ", ");
}
template <typename T> string v2s(const vector<pair<string, T>> &targs) {
  vector<string> args;
  for (auto &t : targs)
    args.push_back(t.second->toString());
  return join(args, ", ");
}

ExprPtr TransformVisitor::conditionalMagic(const ExprPtr &expr,
                                           const string &type,
                                           const string &magic) {
  auto e = transform(expr);
  if (e->getType()->getUnbound())
    return e;
  if (auto c = e->getType()->getClass()) {
    if (chop(c->name) == type)
      return e;
    return transform(
        Nx<CallExpr>(e.get(), Nx<DotExpr>(e.get(), expr->clone(), magic)));
  } else {
    error(e, "cannot find magic '{}' in {}", magic, e->getType()->toString());
  }
  return nullptr;
}

ExprPtr TransformVisitor::makeBoolExpr(const ExprPtr &e) {
  return conditionalMagic(e, "bool", "__bool__");
}

TransformVisitor::TransformVisitor(shared_ptr<TypeContext> ctx,
                                   shared_ptr<vector<StmtPtr>> stmts)
    : ctx(ctx) {
  prependStmts = stmts ? stmts : make_shared<vector<StmtPtr>>();
}

void TransformVisitor::prepend(StmtPtr s) {
  if (auto t = transform(s))
    prependStmts->push_back(move(t));
}

ExprPtr TransformVisitor::transform(const Expr *expr, bool allowTypes) {
  if (!expr)
    return nullptr;
  TransformVisitor v(ctx, prependStmts);
  v.setSrcInfo(expr->getSrcInfo());
  LOG9("[ {} :- {} # {}", *expr,
       expr->getType() ? expr->getType()->toString() : "-",
       expr->getSrcInfo().line);
  __level__++;
  expr->accept(v);
  __level__--;
  LOG9("  {} :- {} ]", *v.resultExpr,
       v.resultExpr->getType() ? v.resultExpr->getType()->toString() : "-");
  if (v.resultExpr && v.resultExpr->getType() &&
      v.resultExpr->getType()->canRealize()) {
    if (auto c = v.resultExpr->getType()->getClass())
      // if (!c->getFunc())
      realizeType(c);
  }
  if (!allowTypes && v.resultExpr && v.resultExpr->isType())
    error("unexpected type expression");
  return move(v.resultExpr);
}

ExprPtr TransformVisitor::transformType(const ExprPtr &expr) {
  auto e = transform(expr.get(), true);
  if (e && !e->isType())
    error("expected type expression");
  return e;
}

/*************************************************************************************/

// Transformed
void TransformVisitor::visit(const NoneExpr *expr) {
  resultExpr = transform(N<CallExpr>(N<IdExpr>("optional")));
}

void TransformVisitor::visit(const BoolExpr *expr) {
  resultExpr = expr->clone();
  resultExpr->setType(forceUnify(resultExpr, ctx->findInternal("bool")));
}

void TransformVisitor::visit(const IntExpr *expr) {
  resultExpr = expr->clone();
  resultExpr->setType(forceUnify(resultExpr, ctx->findInternal("int")));
}

void TransformVisitor::visit(const FloatExpr *expr) {
  resultExpr = expr->clone();
  resultExpr->setType(forceUnify(resultExpr, ctx->findInternal("float")));
}

void TransformVisitor::visit(const StringExpr *expr) {
  resultExpr = expr->clone();
  resultExpr->setType(forceUnify(resultExpr, ctx->findInternal("str")));
}

// Transformed
void TransformVisitor::visit(const FStringExpr *expr) {
  int braceCount = 0, braceStart = 0;
  vector<ExprPtr> items;
  for (int i = 0; i < expr->value.size(); i++) {
    if (expr->value[i] == '{') {
      if (braceStart < i)
        items.push_back(
            N<StringExpr>(expr->value.substr(braceStart, i - braceStart)));
      if (!braceCount)
        braceStart = i + 1;
      braceCount++;
    } else if (expr->value[i] == '}') {
      braceCount--;
      if (!braceCount) {
        string code = expr->value.substr(braceStart, i - braceStart);
        auto offset = expr->getSrcInfo();
        offset.col += i;
        if (code.size() && code.back() == '=') {
          code = code.substr(0, code.size() - 1);
          items.push_back(N<StringExpr>(format("{}=", code)));
        }
        items.push_back(N<CallExpr>(N<IdExpr>("str"), parseExpr(code, offset)));
      }
      braceStart = i + 1;
    }
  }
  if (braceCount)
    error("f-string braces are not balanced");
  if (braceStart != expr->value.size())
    items.push_back(N<StringExpr>(
        expr->value.substr(braceStart, expr->value.size() - braceStart)));
  resultExpr = transform(N<CallExpr>(N<DotExpr>(N<IdExpr>("str"), "cat"),
                                     N<ListExpr>(move(items))));
}

// Transformed
void TransformVisitor::visit(const KmerExpr *expr) {
  resultExpr = transform(N<CallExpr>(
      N<IndexExpr>(N<IdExpr>("Kmer"), N<IntExpr>(expr->value.size())),
      N<SeqExpr>(expr->value)));
}

// Transformed
void TransformVisitor::visit(const SeqExpr *expr) {
  if (expr->prefix == "p")
    resultExpr =
        transform(N<CallExpr>(N<IdExpr>("pseq"), N<StringExpr>(expr->value)));
  else if (expr->prefix == "s")
    resultExpr =
        transform(N<CallExpr>(N<IdExpr>("seq"), N<StringExpr>(expr->value)));
  else
    error("invalid prefix '{}'", expr->prefix);
}

shared_ptr<TypeItem::Item>
TransformVisitor::processIdentifier(shared_ptr<TypeContext> tctx,
                                    const string &id) {
  auto val = tctx->find(id);
  if (!val ||
      (val->getVar() && !val->isGlobal() && val->getBase() != ctx->getBase()))
    return nullptr;
  return val;
}

string TransformVisitor::patchIfRealizable(TypePtr typ, bool isClass) {
  // Patch the name if it can be realized
  if (typ->canRealize()) {
    if (isClass && typ->canRealize()) {
      auto r = realizeType(typ->getClass());
      forceUnify(typ, r.type);
      return r.fullName;
    } else if (typ->getFunc()) {
      auto r = realizeFunc(typ->getFunc());
      forceUnify(typ, r.type);
      return r.fullName;
    }
  }
  return "";
}

void TransformVisitor::visit(const IdExpr *expr) {
  resultExpr = expr->clone();
  auto val = processIdentifier(ctx, expr->value);
  if (!val)
    error("identifier '{}' not found", expr->value);
  if (auto s = val->getStatic()) {
    resultExpr = transform(N<IntExpr>(s->getValue()));
  } else {
    if (val->getClass())
      resultExpr->markType();
    auto typ = val->getImport()
                   ? make_shared<types::ImportType>(val->getImport()->getFile())
                   : ctx->instantiate(getSrcInfo(), val->getType());
    resultExpr->setType(forceUnify(resultExpr, typ));

    auto newName = patchIfRealizable(typ, val->getClass());
    if (!newName.empty())
      static_cast<IdExpr *>(resultExpr.get())->value = newName;
  }
}

// Transformed
void TransformVisitor::visit(const UnpackExpr *expr) {
  resultExpr = transform(N<CallExpr>(N<IdExpr>("list"), expr->what->clone()));
}

// Transformed
void TransformVisitor::visit(const TupleExpr *expr) {
  auto name = generateVariardicStub("tuple", expr->items.size());
  resultExpr = transform(N<CallExpr>(N<DotExpr>(N<IdExpr>(name), "__new__"),
                                     transform(expr->items)));
}

// Transformed
void TransformVisitor::visit(const ListExpr *expr) {
  string listVar = getTemporaryVar("lst");
  prepend(N<AssignStmt>(
      N<IdExpr>(listVar),
      N<CallExpr>(N<IdExpr>("list"), expr->items.size()
                                         ? N<IntExpr>(expr->items.size())
                                         : nullptr)));
  for (int i = 0; i < expr->items.size(); i++)
    prepend(N<ExprStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(listVar), "append"),
                                    expr->items[i]->clone())));
  resultExpr = transform(N<IdExpr>(listVar));
}

// Transformed
void TransformVisitor::visit(const SetExpr *expr) {
  string setVar = getTemporaryVar("set");
  prepend(N<AssignStmt>(N<IdExpr>(setVar), N<CallExpr>(N<IdExpr>("set"))));
  for (int i = 0; i < expr->items.size(); i++)
    prepend(N<ExprStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(setVar), "add"),
                                    expr->items[i]->clone())));
  resultExpr = transform(N<IdExpr>(setVar));
}

// Transformed
void TransformVisitor::visit(const DictExpr *expr) {
  string dictVar = getTemporaryVar("dict");
  prepend(N<AssignStmt>(N<IdExpr>(dictVar), N<CallExpr>(N<IdExpr>("dict"))));
  for (int i = 0; i < expr->items.size(); i++)
    prepend(N<ExprStmt>(N<CallExpr>(
        N<DotExpr>(N<IdExpr>(dictVar), "__setitem__"),
        expr->items[i].key->clone(), expr->items[i].value->clone())));
  resultExpr = transform(N<IdExpr>(dictVar));
}

// Transformed
TransformVisitor::CaptureVisitor::CaptureVisitor(shared_ptr<TypeContext> ctx)
    : ctx(ctx) {}

void TransformVisitor::CaptureVisitor::visit(const IdExpr *expr) {
  auto val = ctx->find(expr->value);
  if (val && val->getVar())
    captures.insert(expr->value);
}

StmtPtr
TransformVisitor::getGeneratorBlock(const vector<GeneratorExpr::Body> &loops,
                                    SuiteStmt *&prev) {
  StmtPtr suite = N<SuiteStmt>(), newSuite = nullptr;
  prev = (SuiteStmt *)suite.get();
  SuiteStmt *nextPrev = nullptr;
  for (auto &l : loops) {
    newSuite = N<SuiteStmt>();
    nextPrev = (SuiteStmt *)newSuite.get();

    vector<ExprPtr> vars;
    for (auto &s : l.vars)
      vars.push_back(N<IdExpr>(s));
    prev->stmts.push_back(
        N<ForStmt>(vars.size() == 1 ? move(vars[0]) : N<TupleExpr>(move(vars)),
                   l.gen->clone(), move(newSuite)));
    prev = nextPrev;
    for (auto &cond : l.conds) {
      newSuite = N<SuiteStmt>();
      nextPrev = (SuiteStmt *)newSuite.get();
      prev->stmts.push_back(N<IfStmt>(cond->clone(), move(newSuite)));
      prev = nextPrev;
    }
  }
  return suite;
}

// Transformed
void TransformVisitor::visit(const GeneratorExpr *expr) {
  SuiteStmt *prev;
  auto suite = getGeneratorBlock(expr->loops, prev);
  string var = getTemporaryVar("gen");
  if (expr->kind == GeneratorExpr::ListGenerator) {
    prepend(N<AssignStmt>(N<IdExpr>(var), N<CallExpr>(N<IdExpr>("list"))));
    prev->stmts.push_back(N<ExprStmt>(N<CallExpr>(
        N<DotExpr>(N<IdExpr>(var), "append"), expr->expr->clone())));
    prepend(move(suite));
  } else if (expr->kind == GeneratorExpr::SetGenerator) {
    prepend(N<AssignStmt>(N<IdExpr>(var), N<CallExpr>(N<IdExpr>("set"))));
    prev->stmts.push_back(N<ExprStmt>(N<CallExpr>(
        N<DotExpr>(N<IdExpr>(var), "insert"), expr->expr->clone())));
    prepend(move(suite));
  } else {
    CaptureVisitor cv(ctx);
    expr->expr->accept(cv);

    prev->stmts.push_back(N<YieldStmt>(expr->expr->clone()));
    string fnVar = getTemporaryVar("anonGen");

    vector<Param> captures;
    for (auto &c : cv.captures)
      captures.push_back({c, nullptr, nullptr});
    prepend(N<FunctionStmt>(fnVar, nullptr, vector<Param>{}, move(captures),
                            move(suite), vector<string>{}));
    vector<CallExpr::Arg> args;
    for (auto &c : cv.captures)
      args.push_back({c, nullptr});
    prepend(
        N<AssignStmt>(N<IdExpr>(var),
                      N<CallExpr>(N<IdExpr>("iter"),
                                  N<CallExpr>(N<IdExpr>(fnVar), move(args)))));
  }
  resultExpr = transform(N<IdExpr>(var));
}

// Transformed
void TransformVisitor::visit(const DictGeneratorExpr *expr) {
  SuiteStmt *prev;
  auto suite = getGeneratorBlock(expr->loops, prev);
  string var = getTemporaryVar("gen");
  prepend(N<AssignStmt>(N<IdExpr>(var), N<CallExpr>(N<IdExpr>("dict"))));
  prev->stmts.push_back(
      N<ExprStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "__setitem__"),
                              expr->key->clone(), expr->expr->clone())));
  prepend(move(suite));
  resultExpr = transform(N<IdExpr>(var));
}

void TransformVisitor::visit(const IfExpr *expr) {
  auto e = N<IfExpr>(makeBoolExpr(expr->cond), transform(expr->eif),
                     transform(expr->eelse));
  e->setType(forceUnify(expr, e->eif->getType()));
  resultExpr = move(e);
}

// Transformed
void TransformVisitor::visit(const UnaryExpr *expr) {
  if (expr->op == "!") { // Special case
    auto e = N<UnaryExpr>(expr->op, makeBoolExpr(expr->expr));
    e->setType(forceUnify(expr, ctx->findInternal("bool")));
    resultExpr = move(e);
  } else {
    string magic;
    if (expr->op == "~")
      magic = "invert";
    else if (expr->op == "+")
      magic = "pos";
    else if (expr->op == "-")
      magic = "neg";
    else
      error("invalid unary operator '{}'", expr->op);
    magic = format("__{}__", magic);
    resultExpr = transform(N<CallExpr>(N<DotExpr>(expr->expr->clone(), magic)));
  }
}

// Transformed
void TransformVisitor::visit(const BinaryExpr *expr) {
  auto magics = unordered_map<string, string>{
      {"+", "add"},     {"-", "sub"},  {"*", "mul"},     {"**", "pow"},
      {"/", "truediv"}, {"//", "div"}, {"@", "mathmul"}, {"%", "mod"},
      {"<", "lt"},      {"<=", "le"},  {">", "gt"},      {">=", "ge"},
      {"==", "eq"},     {"!=", "ne"},  {"<<", "lshift"}, {">>", "rshift"},
      {"&", "and"},     {"|", "or"},   {"^", "xor"}};
  if (expr->op == "&&" || expr->op == "||") { // Special case
    auto e = N<BinaryExpr>(makeBoolExpr(expr->lexpr), expr->op,
                           makeBoolExpr(expr->rexpr));
    e->setType(forceUnify(expr, ctx->findInternal("bool")));
    resultExpr = move(e);
  } else if (expr->op == "is") {
    // check is Type or raw!
    // auto l = transform(expr->lexpr), r = transform(expr->rexpr);
    // if (r->isType()) {
    // }
    auto e =
        N<BinaryExpr>(transform(expr->lexpr), expr->op, transform(expr->rexpr));
    e->setType(forceUnify(expr, ctx->findInternal("bool")));
    resultExpr = move(e);
  } else if (expr->op == "is not") {
    resultExpr = transform(N<UnaryExpr>(
        "!", N<BinaryExpr>(expr->lexpr->clone(), "is", expr->rexpr->clone())));
  } else if (expr->op == "not in") {
    resultExpr = transform(N<UnaryExpr>(
        "!", N<BinaryExpr>(expr->lexpr->clone(), "in", expr->rexpr->clone())));
  } else if (expr->op == "in") {
    resultExpr =
        transform(N<CallExpr>(N<DotExpr>(expr->lexpr->clone(), "__contains__"),
                              expr->rexpr->clone()));
  } else {
    auto le = transform(expr->lexpr);
    auto re = transform(expr->rexpr);
    if (le->getType()->getUnbound() || re->getType()->getUnbound()) {
      resultExpr = N<BinaryExpr>(move(le), expr->op, move(re));
      resultExpr->setType(ctx->addUnbound(getSrcInfo()));
    } else {
      auto mi = magics.find(expr->op);
      if (mi == magics.end())
        error("invalid binary operator '{}'", expr->op);
      auto magic = mi->second;
      auto lc = le->getType()->getClass(), rc = re->getType()->getClass();
      assert(lc && rc);
      if (findBestCall(lc, format("__{}__", magic), {{"", lc}, {"", rc}})) {
        if (expr->inPlace &&
            findBestCall(lc, format("__i{}__", magic), {{"", lc}, {"", rc}}))
          magic = "i" + magic;
      } else if (findBestCall(rc, format("__r{}__", magic),
                              {{"", rc}, {"", lc}})) {
        magic = "r" + magic;
      } else {
        error("cannot find magic '{}' for {}", magic, lc->toString());
      }
      magic = format("__{}__", magic);
      resultExpr = transform(N<CallExpr>(
          N<DotExpr>(expr->lexpr->clone(), magic), expr->rexpr->clone()));
    }
  }
}

void TransformVisitor::fixExprName(ExprPtr &e, const string &newName) {
  if (auto i = CAST(e, CallExpr)) // partial calls
    fixExprName(i->expr, newName);
  else if (auto i = CAST(e, IdExpr))
    i->value = newName;
  else if (auto i = CAST(e, DotExpr))
    i->member = newName;
  else {
    LOG7("fixing {}", *e);
    assert(false);
  }
}

void TransformVisitor::visit(const PipeExpr *expr) {
  auto extractType = [&](TypePtr t) {
    auto c = t->getClass();
    if (c && c->name == "generator")
      return c->explicits[0].type;
    else
      return t;
  };
  auto updateType = [&](TypePtr t, int inTypePos, ExprPtr &fe) {
    auto f = fe->getType()->getClass();
    assert(f && f->getCallable());
    // exactly one empty slot!
    forceUnify(t, f->args[inTypePos + 1]);
    if (f->canRealize() && f->getFunc()) {
      auto r = realizeFunc(f->getFunc());
      forceUnify(f, r.type);
      fixExprName(fe, r.fullName);
    }
    return f->args[0];
  };

  vector<PipeExpr::Pipe> items;
  items.push_back({expr->items[0].op, transform(expr->items[0].expr)});
  TypePtr inType = extractType(items.back().expr->getType());
  int inTypePos = 0;
  for (int i = 1; i < expr->items.size(); i++) {
    auto &l = expr->items[i];
    if (auto ce = CAST(l.expr, CallExpr)) {
      int inTypePos = -1;
      for (int ia = 0; ia < ce->args.size(); ia++)
        if (CAST(ce->args[ia].value, EllipsisExpr)) {
          if (inTypePos == -1)
            inTypePos = ia;
          else
            error(ce->args[ia].value, "unexpected partial argument");
        }
      if (inTypePos == -1) {
        ce->args.insert(ce->args.begin(), {"", N<EllipsisExpr>()});
        inTypePos = 0;
      }
      items.push_back({l.op, transform(ce)});
    } else {
      items.push_back(
          {l.op, transform(N<CallExpr>(transform(l.expr), N<EllipsisExpr>()))});
      inTypePos = 0;
    }
    inType = updateType(inType, inTypePos, items.back().expr);
    if (i < expr->items.size() - 1)
      inType = extractType(inType);
  }
  resultExpr = N<PipeExpr>(move(items));
  resultExpr->setType(forceUnify(expr, inType));
}

void TransformVisitor::visit(const IndexExpr *expr) {
  vector<TypePtr> generics;
  auto parseGeneric = [&](const ExprPtr &i) {
    auto ti = transform(i, true);
    if (auto ii = CAST(ti, IntExpr))
      generics.push_back(make_shared<StaticType>(std::stoll(ii->value)));
    else if (ti->isType())
      generics.push_back(ti->getType());
    else
      error(ti, "expected type expression");
  };

  // special cases: tuples and functions
  ExprPtr e = nullptr;
  if (auto i = CAST(expr->expr, IdExpr)) {
    if (i->value == "tuple" || i->value == "function") {
      auto t = CAST(expr->index, TupleExpr);
      auto name = generateVariardicStub(i->value, t ? t->items.size() : 1);
      e = transform(N<IdExpr>(name), true);
    }
  }
  if (!e)
    e = transform(expr->expr, true);
  // Type or function realization (e.g. dict[type1, type2])
  if (e->isType() || (e->getType()->getFunc())) {
    if (auto t = CAST(expr->index, TupleExpr))
      for (auto &i : t->items)
        parseGeneric(i);
    else
      parseGeneric(expr->index);
    auto g = e->getType()->getClass();
    if (g->explicits.size() != generics.size())
      error("expected {} generics, got {}", g->explicits.size(),
            generics.size());
    for (int i = 0; i < generics.size(); i++)
      forceUnify(g->explicits[i].type, generics[i]);
    auto t = e->getType();
    bool isType = e->isType();
    t = forceUnify(expr, t);
    auto newName = patchIfRealizable(t, isType);
    if (!newName.empty())
      fixExprName(e, newName);

    resultExpr = move(e); // will get replaced by identifier later on
    if (isType)
      resultExpr->markType();
    resultExpr->setType(t);
  } else {
    if (auto c = e->getType()->getClass())
      if (chop(c->name).substr(0, 8) == "__tuple_") {
        if (auto ii = CAST(expr->index, IntExpr)) {
          resultExpr = transform(
              N<TupleIndexExpr>(expr->expr->clone(), std::stoll(ii->value)));
          return;
        }
      }
    resultExpr = transform(N<CallExpr>(
        N<DotExpr>(expr->expr->clone(), "__getitem__"), expr->index->clone()));
  }
}

void TransformVisitor::visit(const TupleIndexExpr *expr) {
  auto e = transform(expr->expr);
  auto c = e->getType()->getClass();
  assert(chop(c->name).substr(0, 8) == "__tuple_");
  if (expr->index < 0 || expr->index >= c->args.size())
    error("tuple index out of range (expected 0..{}, got {})",
          c->args.size() - 1, expr->index);
  resultExpr = N<TupleIndexExpr>(move(e), expr->index);
  resultExpr->setType(forceUnify(expr, c->args[expr->index]));
}

void TransformVisitor::visit(const StackAllocExpr *expr) {
  auto te = transformType(expr->typeExpr);
  auto e = transform(expr->expr);
  auto t = ctx->instantiateGeneric(expr->getSrcInfo(),
                                   ctx->findInternal("array"), {te->getType()});
  patchIfRealizable(t, true);
  resultExpr = N<StackAllocExpr>(move(te), move(e));
  resultExpr->setType(forceUnify(expr, t));
}

string TransformVisitor::generateVariardicStub(const string &name, int len) {
  // TODO: handle name clashes (add special name character?)
  auto typeName = fmt::format("__{}_{}", name, len);
  assert(len >= 1);
  if (ctx->getRealizations()->variardicCache.find(typeName) ==
      ctx->getRealizations()->variardicCache.end()) {
    ctx->getRealizations()->variardicCache.insert(typeName);
    vector<string> generics, args;
    for (int i = 1; i <= len; i++) {
      generics.push_back(format("T{}", i));
      args.push_back(format("a{0}: T{0}", i));
    }
    auto code = format("type {}[{}]({})", typeName, join(generics, ", "),
                       join(args, ", "));

    if (name == "function") {
      code = format("@internal\n{0}:\n  @internal\n  def __str__(self: "
                    "function[{1}]) -> str: pass\n "
                    " @internal\n  "
                    "def __new__(what: ptr[byte]) -> function[{1}]: pass\n",
                    code, join(generics, ", "));
    } else if (name == "tuple") {
      string str;
      str = "  def __str__(self) -> str:\n";
      str += format("    s = list[str]({})\n", len + 2);
      str += format("    s.append('(')\n");
      for (int i = 0; i < len; i++) {
        str += format("    s.append(self[{}].__str__())\n", i);
        str += format("    s.append('{}')\n", i == len - 1 ? ")" : ", ");
      }
      str += "    return str.cat(s)\n";
      code += ":\n" + str;
    } else if (name != "partial") {
      error("invalid variardic type");
    }
    LOG7("[VAR] generating {}...\n{}", typeName, code);

    auto a = parseCode(ctx->getFilename(), code);
    auto i = ctx->getImports()->getImport("");
    auto stmtPtr = dynamic_cast<SuiteStmt *>(i->statements.get());
    assert(stmtPtr);

    auto nc = make_shared<TypeContext>(i->tctx->getFilename(),
                                       i->tctx->getRealizations(),
                                       i->tctx->getImports());
    stmtPtr->stmts.push_back(TransformVisitor(nc).transform(a));
    for (auto &ax : *nc)
      i->tctx->addToplevel(ax.first, ax.second.front());
  }
  return typeName;
}

FuncTypePtr
TransformVisitor::findBestCall(ClassTypePtr c, const string &member,
                               const vector<pair<string, TypePtr>> &args,
                               bool failOnMultiple, TypePtr retType) {
  auto m = ctx->getRealizations()->findMethod(c->name, member);
  if (!m)
    return nullptr;

  if (m->size() == 1) // works
    return (*m)[0];

  // TODO: For now, overloaded functions are only possible in magic methods
  // Another assomption is that magic methods of interest have no default
  // arguments or reordered arguments...
  if (member.substr(0, 2) != "__" || member.substr(member.size() - 2) != "__") {
    error("overloaded non-magic method...");
  }
  for (auto &a : args)
    if (!a.first.empty())
      error("[todo] named magic call");

  vector<pair<int, int>> scores;
  for (int i = 0; i < m->size(); i++) {
    auto mt = dynamic_pointer_cast<FuncType>(
        ctx->instantiate(getSrcInfo(), (*m)[i], c, false));
    auto s = 0;
    if (mt->args.size() - 1 != args.size())
      continue;
    for (int j = 0; j < args.size(); j++) {
      Unification us;
      int u = args[j].second->unify(mt->args[j + 1], us);
      us.undo();
      if (u < 0) {
        s = -1;
        break;
      } else {
        s += u;
      }
    }
    if (retType) {
      Unification us;
      int u = retType->unify(mt->args[0], us);
      us.undo();
      s = u < 0 ? -1 : s + u;
    }
    if (s >= 0)
      scores.push_back({s, i});
  }
  if (!scores.size())
    return nullptr;
  sort(scores.begin(), scores.end(), std::greater<pair<int, int>>());
  if (failOnMultiple) {
    for (int i = 1; i < scores.size(); i++)
      if (scores[i].first == scores[0].first)
        // return nullptr;
        compilationWarning(
            format("multiple choices for magic call, selected {}",
                   (*m)[scores[0].second]->canonicalName),
            getSrcInfo().file, getSrcInfo().line);
      else
        break;
  }
  return (*m)[scores[0].second];
}

vector<int>
TransformVisitor::callCallable(types::ClassTypePtr f,
                               vector<CallExpr::Arg> &args,
                               vector<CallExpr::Arg> &reorderedArgs) {
  assert(f->getCallable());
  bool isPartial = false;
  if (args.size() != f->args.size() - 1) {
    if (args.size() == f->args.size() &&
        CAST(args.back().value, EllipsisExpr)) {
      isPartial = true;
      args.pop_back();
    } else {
      error("too many arguments for {} (expected {}, got {})", f->toString(),
            f->args.size() - 1, args.size());
    }
  }
  vector<int> pending;
  for (int i = 0; i < args.size(); i++) {
    if (args[i].name != "")
      error("argument '{}' missing (function pointers have argument "
            "names elided)",
            args[i].name);
    reorderedArgs.push_back({"", move(args[i].value)});

    forceUnify(reorderedArgs[i].value, f->args[i + 1]);
    if (CAST(reorderedArgs[i].value, EllipsisExpr))
      pending.push_back(i);
  }
  if (isPartial || pending.size())
    pending.push_back(args.size());
  return pending;
}

vector<int> TransformVisitor::callFunc(types::FuncTypePtr f,
                                       vector<CallExpr::Arg> &args,
                                       vector<CallExpr::Arg> &reorderedArgs) {
  vector<int> pending;
  bool isPartial = false;
  bool namesStarted = false;
  unordered_map<string, ExprPtr> namedArgs;
  for (int i = 0; i < args.size(); i++) {
    if (args[i].name == "" && namesStarted)
      error("unnamed argument after a named argument");
    namesStarted |= args[i].name != "";
    if (args[i].name == "")
      reorderedArgs.push_back({"", move(args[i].value)});
    else if (namedArgs.find(args[i].name) == namedArgs.end())
      namedArgs[args[i].name] = move(args[i].value);
    else
      error("named argument {} repeated multiple times", args[i].name);
  }
  if (namedArgs.size() == 0 && reorderedArgs.size() == f->args.size() &&
      CAST(reorderedArgs.back().value, EllipsisExpr)) {
    isPartial = true;
    reorderedArgs.pop_back();
  } else if (reorderedArgs.size() + namedArgs.size() > f->args.size() - 1) {
    error("too many arguments for {} (expected {}, got {})", f->toString(),
          f->args.size() - 1, reorderedArgs.size() + namedArgs.size());
  }
  for (int i = 0, ra = reorderedArgs.size(); i < f->args.size() - 1; i++) {
    if (i >= ra) {
      auto it = namedArgs.find(f->argDefs[i].name);
      if (it != namedArgs.end()) {
        reorderedArgs.push_back({"", move(it->second)});
        namedArgs.erase(it);
      } else if (f->argDefs[i].defaultValue) {
        reorderedArgs.push_back({"", transform(f->argDefs[i].defaultValue)});
      } else {
        error("argument '{}' missing", f->argDefs[i].name);
      }
    }
    if (CAST(reorderedArgs[i].value, EllipsisExpr))
      pending.push_back(i);
    // forceUnify(reorderedArgs[i].value, f->argDefs[i].type);
    forceUnify(reorderedArgs[i].value, f->args[i + 1]);
  }
  for (auto &i : namedArgs)
    error(i.second, "unknown argument {}", i.first);
  if (isPartial || pending.size())
    pending.push_back(args.size());
  return pending;
}

vector<int>
TransformVisitor::callPartial(types::PartialTypePtr f,
                              vector<CallExpr::Arg> &args,
                              vector<CallExpr::Arg> &reorderedArgs) {
  // TODO: parse named arguments for partial functions
  bool isPartial = false;
  if (args.size() != f->pending.size()) {
    if (args.size() == f->pending.size() + 1 &&
        CAST(args.back().value, EllipsisExpr)) {
      isPartial = true;
      args.pop_back();
    } else {
      error("too many arguments for {} (expected {}, got {})", f->toString(),
            f->pending.size(), args.size());
    }
  }
  vector<int> pending;
  for (int i = 0; i < args.size(); i++) {
    if (args[i].name != "")
      error("argument '{}' missing (partial calls have argument "
            "names elided)",
            args[i].name);
    reorderedArgs.push_back({"", move(args[i].value)});
    forceUnify(reorderedArgs[i].value, f->args[f->pending[i] + 1]);
    if (CAST(reorderedArgs[i].value, EllipsisExpr))
      pending.push_back(f->pending[i]);
  }
  if (isPartial || pending.size())
    pending.push_back(args.size());
  return pending;
}

bool TransformVisitor::handleStackAlloc(const CallExpr *expr) {
  if (auto ix = CAST(expr->expr, IndexExpr)) {
    if (auto id = CAST(ix->expr, IdExpr)) {
      if (id->value == "__array__") {
        if (expr->args.size() != 1)
          error("__array__ requires only size argument");
        resultExpr = transform(N<StackAllocExpr>(ix->index->clone(),
                                                 expr->args[0].value->clone()));
        return true;
      }
    }
  }
  return false;
}

void TransformVisitor::visit(const CallExpr *expr) {
  if (handleStackAlloc(expr))
    return;

  ExprPtr e = nullptr;
  vector<CallExpr::Arg> args;
  for (auto &i : expr->args)
    args.push_back({i.name, transform(i.value)});

  // Intercept obj.foo() calls and transform obj.foo(...) to foo(obj, ...)
  if (auto d = CAST(expr->expr, DotExpr)) {
    auto dotlhs = transform(d->expr, true);
    if (auto c = dotlhs->getType()->getClass()) {
      vector<pair<string, TypePtr>> targs;
      if (!dotlhs->isType())
        targs.push_back({"", c});
      for (auto &a : args)
        targs.push_back({a.name, a.value->getType()});
      if (auto m = findBestCall(c, d->member, targs, true)) {
        if (!dotlhs->isType())
          args.insert(args.begin(), {"", move(dotlhs)});
        e = N<IdExpr>(m->canonicalName);
        e->setType(ctx->instantiate(getSrcInfo(), m, c));
      } else
        error("cannot find method '{}' in {} with arguments {}", d->member,
              c->toString(), v2s(targs));
    }
  }
  if (!e)
    e = transform(expr->expr, true);
  forceUnify(expr->expr.get(), e->getType());

  // TODO: optional promition in findBestCall
  if (e->isType()) { // Replace constructor with appropriate calls
    auto c = e->getType()->getClass();
    assert(c);
    if (c->isRecord()) {
      vector<TypePtr> targs;
      for (auto &a : args)
        targs.push_back(a.value->getType());
      resultExpr =
          transform(N<CallExpr>(N<DotExpr>(move(e), "__new__"), move(args)));
    } else {
      string var = getTemporaryVar("typ");
      /// TODO: assumes that a class cannot have multiple __new__ magics
      /// WARN: passing e & args that have already been transformed
      prepend(N<AssignStmt>(N<IdExpr>(var),
                            N<CallExpr>(N<DotExpr>(move(e), "__new__"))));
      prepend(N<ExprStmt>(
          N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "__init__"), move(args))));
      resultExpr = transform(N<IdExpr>(var));
    }
    return;
  }

  auto c = e->getType()->getClass();
  if (!c) { // Unbound caller, will be handled later
    resultExpr = N<CallExpr>(move(e), move(args));
    resultExpr->setType(expr->getType() ? expr->getType()
                                        : ctx->addUnbound(getSrcInfo()));
    return;
  }
  if (c && !c->getCallable()) { // route to a call method
    resultExpr =
        transform(N<CallExpr>(N<DotExpr>(move(e), "__call__"), move(args)));
    return;
  }

  // Handle named and default arguments
  vector<CallExpr::Arg> reorderedArgs;
  vector<int> pending;
  if (auto f = c->getFunc())
    pending = callFunc(f, args, reorderedArgs);
  else if (auto f = dynamic_pointer_cast<types::PartialType>(c))
    pending = callPartial(f, args, reorderedArgs);
  else
    pending = callCallable(c, args, reorderedArgs);

  // Realize functions that are passed as arguments
  for (auto &ra : reorderedArgs)
    if (auto f = ra.value->getType()->getFunc()) {
      if (f->canRealize()) {
        auto r = realizeFunc(f);
        forceUnify(f, r.type);
        fixExprName(ra.value, r.fullName);
      }
    }

  if (pending.size()) {
    pending.pop_back();
    auto t = make_shared<PartialType>(c, pending);
    generateVariardicStub("partial", pending.size());
    forceUnify(expr, t);
    if (t->canRealize() && c->getFunc()) {
      auto r = realizeFunc(c->getFunc());
      forceUnify(t, r.type);
      fixExprName(e, r.fullName);
    }
    resultExpr = N<CallExpr>(move(e), move(reorderedArgs));
    resultExpr->setType(t);
  } else {
    if (c->canRealize() && c->getFunc()) {
      auto r = realizeFunc(c->getFunc());
      forceUnify(c, r.type);
      fixExprName(e, r.fullName);
    }
    resultExpr = N<CallExpr>(move(e), move(reorderedArgs));
    resultExpr->setType(forceUnify(expr, make_shared<LinkType>(c->args[0])));
  }
}

void TransformVisitor::visit(const DotExpr *expr) {
  // Handle import chains separately
  const ExprPtr *e = &(expr->expr);
  deque<string> chain;
  while (auto d = dynamic_cast<DotExpr *>(e->get())) {
    chain.push_front(d->member);
    e = &(d->expr);
  }
  if (auto d = dynamic_cast<IdExpr *>(e->get())) {
    chain.push_front(d->value);
    auto s = join(chain, "/");
    auto val = ctx->find(s);
    if (val && val->getImport()) {
      resultExpr = N<DotExpr>(N<IdExpr>(s), expr->member);
      auto ictx =
          ctx->getImports()->getImport(val->getImport()->getFile())->tctx;
      auto ival = processIdentifier(ictx, expr->member);
      if (!ival)
        error("identifier '{}' not found", expr->member);
      if (ival->getClass())
        resultExpr->markType();
      resultExpr->setType(
          forceUnify(expr, ctx->instantiate(getSrcInfo(), ival->getType())));

      auto newName = patchIfRealizable(resultExpr->getType(), ival->getClass());
      if (!newName.empty())
        static_cast<DotExpr *>(resultExpr.get())->member = newName;
      return;
    }
  }

  auto lhs = transform(expr->expr, true);
  TypePtr typ = nullptr;
  if (lhs->getType()->getUnbound()) {
    typ = expr->getType() ? expr->getType() : ctx->addUnbound(getSrcInfo());
  } else if (auto c = lhs->getType()->getClass()) {
    if (auto m = ctx->getRealizations()->findMethod(c->name, expr->member)) {
      if (m->size() > 1)
        error("ambigious partial expression"); /// TODO
      if (lhs->isType()) {
        auto name = (*m)[0]->canonicalName;
        auto val = processIdentifier(ctx, name);
        auto t = ctx->instantiate(getSrcInfo(), (*m)[0], c);
        resultExpr = N<IdExpr>(name);
        resultExpr->setType(t);
        auto newName = patchIfRealizable(t, val->getClass());
        if (!newName.empty())
          static_cast<IdExpr *>(resultExpr.get())->value = newName;
        return;
      } else { // cast y.foo to CLS.foo(y, ...)
        auto f = (*m)[0];
        vector<ExprPtr> args;
        args.push_back(move(lhs));
        for (int i = 0; i < std::max(1, (int)f->args.size() - 2); i++)
          args.push_back(N<EllipsisExpr>());

        auto ast = dynamic_cast<FunctionStmt *>(
            ctx->getRealizations()->getAST(f->canonicalName).get());
        assert(ast);
        if (in(ast->attributes, "property"))
          args.pop_back();
        resultExpr = transform(
            N<CallExpr>(N<IdExpr>((*m)[0]->canonicalName), move(args)));
        return;
      }
    } else if (auto mm =
                   ctx->getRealizations()->findMember(c->name, expr->member)) {
      typ = ctx->instantiate(getSrcInfo(), mm, c);
    } else {
      error("cannot find '{}' in {}", expr->member, lhs->getType()->toString());
    }
  } else {
    error("cannot find '{}' in {}", expr->member, lhs->getType()->toString());
  }
  resultExpr = N<DotExpr>(move(lhs), expr->member);
  resultExpr->setType(forceUnify(expr, typ));
}

// Transformation
void TransformVisitor::visit(const SliceExpr *expr) {
  string prefix;
  if (!expr->st && expr->ed)
    prefix = "l";
  else if (expr->st && !expr->ed)
    prefix = "r";
  else if (!expr->st && !expr->ed)
    prefix = "e";
  if (expr->step)
    prefix += "s";

  vector<ExprPtr> args;
  if (expr->st)
    args.push_back(expr->st->clone());
  if (expr->ed)
    args.push_back(expr->ed->clone());
  if (expr->step)
    args.push_back(expr->step->clone());
  if (!args.size())
    args.push_back(N<IntExpr>(0));
  resultExpr = transform(N<CallExpr>(N<IdExpr>(prefix + "slice"), move(args)));
}

void TransformVisitor::visit(const EllipsisExpr *expr) {
  resultExpr = N<EllipsisExpr>();
  resultExpr->setType(ctx->addUnbound(getSrcInfo()));
}

// Should get transformed by other functions
void TransformVisitor::visit(const TypeOfExpr *expr) {
  auto e = transform(expr->expr);
  auto t = forceUnify(expr, e->getType());

  auto newName = patchIfRealizable(t, true);
  if (!newName.empty())
    resultExpr = N<IdExpr>(newName);
  else
    resultExpr = N<TypeOfExpr>(move(e));
  resultExpr->markType();
  resultExpr->setType(t);
}

void TransformVisitor::visit(const PtrExpr *expr) {
  // TODO: force only variables here!
  auto param = transform(expr->expr);
  auto t = ctx->instantiateGeneric(expr->getSrcInfo(), ctx->findInternal("ptr"),
                                   {param->getType()});
  resultExpr = N<PtrExpr>(move(param));
  resultExpr->setType(forceUnify(expr, t));
}

// Transformation
void TransformVisitor::visit(const LambdaExpr *expr) {
  CaptureVisitor cv(ctx);
  expr->expr->accept(cv);

  vector<Param> params;
  unordered_set<string> used;
  for (auto &s : expr->vars) {
    params.push_back({s, nullptr, nullptr});
    used.insert(s);
  }
  for (auto &c : cv.captures)
    if (used.find(c) == used.end())
      params.push_back({c, nullptr, nullptr});

  string fnVar = getTemporaryVar("anonFn");
  prepend(N<FunctionStmt>(fnVar, nullptr, vector<Param>{}, move(params),
                          N<ReturnStmt>(expr->expr->clone()),
                          vector<string>{}));
  vector<CallExpr::Arg> args;
  for (auto &c : cv.captures)
    if (used.find(c) == used.end())
      args.push_back({"", N<IdExpr>(c)});
  if (args.size()) { // create partial call
    for (int i = 0; i < expr->vars.size(); i++)
      args.insert(args.begin(), {"", N<EllipsisExpr>()});
    resultExpr = transform(N<CallExpr>(N<IdExpr>(fnVar), move(args)));
  } else {
    resultExpr = transform(N<IdExpr>(fnVar));
  }
}

// TODO
void TransformVisitor::visit(const YieldExpr *expr) {
  error("yieldexpr is not yet supported");
}

} // namespace ast
} // namespace seq
