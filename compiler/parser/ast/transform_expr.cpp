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

int __level__ = 0;

namespace seq {
namespace ast {

using namespace types;

ExprPtr TransformVisitor::conditionalMagic(const ExprPtr &expr,
                                           const string &type,
                                           const string &magic) {
  auto e = transform(expr);
  if (e->getType()->getUnbound())
    return e;
  if (auto c = e->getType()->getClass()) {
    if (c->name == type)
      return e;
    return transform(
        Nx<CallExpr>(e.get(), Nx<DotExpr>(e.get(), move(e), magic)));
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
  DBG("[ {} :- {} # {}", *expr,
      expr->getType() ? expr->getType()->toString() : "-",
      expr->getSrcInfo().line);
  __level__++;
  expr->accept(v);
  __level__--;
  DBG("  {} :- {} ]", *v.resultExpr,
      v.resultExpr->getType() ? v.resultExpr->getType()->toString() : "-");

  if (v.resultExpr && v.resultExpr->getType() &&
      v.resultExpr->getType()->canRealize()) {
    if (auto c = v.resultExpr->getType()->getClass())
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
    error("identifier '{}' not found", id);
  return val;
}

void TransformVisitor::visit(const IdExpr *expr) {
  resultExpr = expr->clone();
  if (expr->value == "tuple" || expr->value == "function") {
    assert(expr->getType());
    return;
  }
  auto val = processIdentifier(ctx, expr->value);
  if (auto s = val->getStatic()) {
    resultExpr = transform(N<IntExpr>(s->getValue()));
  } else {
    if (val->getClass())
      resultExpr->markType();
    auto typ = val->getImport()
                   ? make_shared<types::ImportType>(val->getImport()->getName())
                   : ctx->instantiate(getSrcInfo(), val->getType());
    resultExpr->setType(forceUnify(resultExpr, typ));
  }
}

// Transformed
void TransformVisitor::visit(const UnpackExpr *expr) {
  resultExpr = transform(N<CallExpr>(N<IdExpr>("list"), expr->what->clone()));
}

void TransformVisitor::visit(const TupleExpr *expr) {
  auto e = N<TupleExpr>(transform(expr->items));
  vector<TypePtr> args;
  for (auto &i : e->items)
    args.push_back(i->getType());
  DBG("{} -> {} ", *expr, *e);
  e->setType(forceUnify(expr, T<ClassType>("tuple", true, args)));
  resultExpr = move(e);
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
      if (findBestCall(lc, format("__{}__", magic), {lc, rc})) {
        if (expr->inPlace &&
            findBestCall(lc, format("__i{}__", magic), {lc, rc}))
          magic = "i" + magic;
      } else if (findBestCall(rc, format("__r{}__", magic), {rc, lc})) {
        magic = "r" + magic;
      } else {
        error("cannot find magic '{}' for {}", magic, lc->toString());
      }
      magic = format("__{}__", magic);
      resultExpr =
          transform(N<CallExpr>(N<DotExpr>(move(le), magic), move(re)));
    }
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
  auto updateType = [&](TypePtr t, int inTypePos, TypePtr ft) {
    auto f = ft->getFunc();
    assert(f);
    // exactly one empty slot!
    forceUnify(t, f->args[inTypePos + 1]);
    if (f->canRealize() && f->realizationInfo)
      forceUnify(f, realizeFunc(f).type);
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
    inType = updateType(inType, inTypePos, items.back().expr->getType());
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
  if (auto i = CAST(expr->expr, IdExpr)) {
    if (i->value == "tuple" || i->value == "function") {
      if (auto t = CAST(expr->index, TupleExpr))
        for (auto &i : t->items)
          parseGeneric(i);
      else
        parseGeneric(expr->index);
      resultExpr = N<IdExpr>(i->value);
      resultExpr->markType();
      if (i->value == "tuple")
        resultExpr->setType(T<ClassType>("tuple", true, generics));
      else
        resultExpr->setType(T<FuncType>(generics));
      return;
    }
  }

  auto e = transform(expr->expr, true);
  // Type or function realization (e.g. dict[type1, type2])
  if (e->isType() || e->getType()->getFunc()) {
    if (auto t = CAST(expr->index, TupleExpr))
      for (auto &i : t->items)
        parseGeneric(i);
    else
      parseGeneric(expr->index);
    auto g = e->getType()->getGeneric();
    if (g->explicits.size() != generics.size())
      error("expected {} generics, got {}", g->explicits.size(),
            generics.size());
    for (int i = 0; i < generics.size(); i++)
      forceUnify(g->explicits[i].type, generics[i]);
    auto t = e->getType();
    bool isType = e->isType();
    resultExpr = move(e); // N<TypeOfExpr>(move(e));
    if (isType)
      resultExpr->markType();
    resultExpr->setType(forceUnify(expr, t));
  } else {
    if (auto c = e->getType()->getClass())
      if (c->name == "tuple") {
        auto i = transform(expr->index);
        if (auto ii = CAST(i, IntExpr)) {
          auto idx = std::stol(ii->value);
          if (idx < 0 || idx >= c->recordMembers.size())
            error(i, "tuple index out of range (expected 0..{}, got {})",
                  c->recordMembers.size(), idx);
          resultExpr = N<IndexExpr>(move(e), move(i));
          resultExpr->setType(forceUnify(expr, c->recordMembers[idx]));
          return;
        }
      }
    resultExpr = transform(
        N<CallExpr>(N<DotExpr>(move(e), "__getitem__"), expr->index->clone()));
  }
}

FuncTypePtr TransformVisitor::findBestCall(ClassTypePtr c, const string &member,
                                           const vector<TypePtr> &args,
                                           bool failOnMultiple) {
  if (c->name == "tuple" && member == "__str__" && args.size() == 1) {
    auto f = make_shared<FuncType>(
        vector<TypePtr>{ctx->findInternal("str"), args[0]});
    f->realizationInfo = make_shared<FuncType::RealizationInfo>(
        "$tuple_str", vector<int>{0},
        vector<FuncType::RealizationInfo::Arg>{{"", args[0], ""}});
    return f;
  }

  auto m = ctx->getRealizations()->findMethod(c->name, member);
  if (!m)
    return nullptr;

  vector<pair<int, int>> scores;
  for (int i = 0; i < m->size(); i++) {
    auto mt = dynamic_pointer_cast<FuncType>(
        ctx->instantiate(getSrcInfo(), (*m)[i], c, false));
    auto s = 0;
    if (mt->args.size() - 1 != args.size())
      continue;
    for (int j = 0; j < args.size(); j++) {
      Unification us;
      int u = args[j]->unify(mt->args[j + 1], us);
      us.undo();
      if (u < 0) {
        s = -1;
        break;
      } else {
        s += u;
      }
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
        return nullptr;
      // compilationWarning(
      //     format("multiple choices for magic call, selected {}",
      //            *(*m)[scores[0].second]),
      //     getSrcInfo().file, getSrcInfo().line);
      else
        break;
  }
  return (*m)[scores[0].second];
}

void TransformVisitor::visit(const CallExpr *expr) {
  /// TODO: wrap pyobj arguments in tuple

  ExprPtr e = nullptr;
  vector<CallExpr::Arg> args;
  for (auto &i : expr->args)
    args.push_back({i.name, transform(i.value)});

  // Intercept obj.foo() calls and transform obj.foo(...) to foo(obj, ...)
  if (auto d = CAST(expr->expr, DotExpr)) {
    auto dotlhs = transform(d->expr, true);
    if (auto c = dotlhs->getType()->getClass()) {
      vector<TypePtr> targs;
      if (!dotlhs->isType())
        targs.push_back(c);
      for (auto &a : args)
        targs.push_back(a.value->getType());
      if (auto m = findBestCall(c, d->member, targs, true)) {
        if (!dotlhs->isType())
          args.insert(args.begin(), {"", move(dotlhs)});
        e = N<IdExpr>(m->realizationInfo->name);
        e->setType(ctx->instantiate(getSrcInfo(), m, c));
      } else {
        vector<string> args;
        for (auto &t : targs)
          args.push_back(t->toString());
        error("cannot find method '{}' in {} with arguments {}", d->member,
              c->toString(), join(args, ","));
      }
    }
  }
  if (!e)
    e = transform(expr->expr, true);
  forceUnify(expr->expr.get(), e->getType());
  if (e->isType()) { // Replace constructor with appropriate calls
    assert(e->getType()->getClass());
    if (e->getType()->getClass()->isRecord()) {
      resultExpr =
          transform(N<CallExpr>(N<DotExpr>(e->clone(), "__new__"), move(args)));
    } else {
      string var = getTemporaryVar("typ");
      prepend(N<AssignStmt>(N<IdExpr>(var),
                            N<CallExpr>(N<DotExpr>(e->clone(), "__new__"))));
      prepend(N<ExprStmt>(
          N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "__init__"), move(args))));
      resultExpr = transform(N<IdExpr>(var));
    }
    return;
  }

  auto f = e->getType()->getFunc();
  if (!f) { // Unbound caller, will be handled later
    resultExpr = N<CallExpr>(move(e), move(args));
    resultExpr->setType(expr->getType() ? expr->getType()
                                        : ctx->addUnbound(getSrcInfo()));
    return;
  }

  vector<CallExpr::Arg> reorderedArgs;
  vector<int> newPending;
  vector<TypePtr> newPendingTypes{f->args[0]};
  bool isPartial = false;
  if (f->realizationInfo) {
    // DBG(" // f :- {}", *f);
    // for (auto &a: f->realizationInfo->args)
    //   DBG(" // args: {}", *a.type);

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
    if (namedArgs.size() == 0 &&
        reorderedArgs.size() == f->realizationInfo->pending.size() + 1 &&
        CAST(reorderedArgs.back().value, EllipsisExpr)) {
      isPartial = true;
      reorderedArgs.pop_back();
    } else if (reorderedArgs.size() + namedArgs.size() >
               f->realizationInfo->pending.size()) {
      error("too many arguments for {} (expected {}, got {}) for {}",
            f->toString(), f->realizationInfo->pending.size(),
            reorderedArgs.size() + namedArgs.size());
    }
    assert(f->args.size() - 1 == f->realizationInfo->pending.size());
    for (int i = 0, ra = reorderedArgs.size();
         i < f->realizationInfo->pending.size(); i++) {
      if (i >= ra) {
        auto it = namedArgs.find(
            f->realizationInfo->args[f->realizationInfo->pending[i]].name);
        if (it != namedArgs.end()) {
          reorderedArgs.push_back({"", move(it->second)});
          namedArgs.erase(it);
        } else if (f->realizationInfo->args[f->realizationInfo->pending[i]]
                       .defaultVar.size()) {
          reorderedArgs.push_back(
              {"", transform(N<IdExpr>(
                       f->realizationInfo->args[f->realizationInfo->pending[i]]
                           .defaultVar))});
        } else {
          error("argument '{}' missing",
                f->realizationInfo->args[f->realizationInfo->pending[i]].name);
          // require explicit ... except for pipes
          // reorderedArgs.push_back({"", transform(N<EllipsisExpr>())});
        }
      }
      if (CAST(reorderedArgs[i].value, EllipsisExpr)) {
        newPending.push_back(f->realizationInfo->pending[i]);
        newPendingTypes.push_back(
            f->realizationInfo->args[f->realizationInfo->pending[i]].type);
        isPartial = true;
      }
      forceUnify(reorderedArgs[i].value,
                 f->realizationInfo->args[f->realizationInfo->pending[i]].type);
      forceUnify(reorderedArgs[i].value, f->args[i + 1]);
    }
    for (auto &i : namedArgs)
      error(i.second, "unknown argument {}", i.first);
  } else { // we only know that it is function[...]; assume it is realized
    if (args.size() != f->args.size() - 1)
      error("too many arguments for {} (expected {}, got {})", *f,
            f->args.size() - 1, args.size());
    for (int i = 0; i < args.size(); i++) {
      if (args[i].name != "")
        error("argument '{}' missing (function pointers have argument "
              "names elided)");
      reorderedArgs.push_back({"", move(args[i].value)});

      forceUnify(reorderedArgs[i].value, f->args[i + 1]);
      if (CAST(reorderedArgs[i].value, EllipsisExpr)) {
        newPending.push_back(i);
        newPendingTypes.push_back(f->args[i + 1]);
        isPartial = true;
      }
    }
  }
  for (auto &r : reorderedArgs)
    if (auto f = r.value->getType()->getFunc())
      if (f->canRealize() && f->realizationInfo)
        forceUnify(f, realizeFunc(f).type);
  if (isPartial) {
    auto t = make_shared<FuncType>(newPendingTypes, f);
    if (f->realizationInfo) {
      t->realizationInfo = make_shared<FuncType::RealizationInfo>(
          f->realizationInfo->name, newPending, f->realizationInfo->args);
    }
    forceUnify(expr, t);
    if (t->canRealize() && t->realizationInfo)
      forceUnify(t, realizeFunc(t).type);
    resultExpr = N<CallExpr>(move(e), move(reorderedArgs));
    resultExpr->setType(t);
  } else {
    if (f->canRealize() && f->realizationInfo)
      forceUnify(f, realizeFunc(f).type);
    resultExpr = N<CallExpr>(move(e), move(reorderedArgs));
    resultExpr->setType(forceUnify(expr, make_shared<LinkType>(f->args[0])));
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
          ctx->getImports()->getImport(val->getImport()->getName())->tctx;
      auto ival = processIdentifier(ictx, expr->member);
      if (ival->getClass())
        resultExpr->markType();
      resultExpr->setType(
          forceUnify(expr, ctx->instantiate(getSrcInfo(), ival->getType())));
      return;
    }
  }

  auto lhs = transform(expr->expr, true);
  TypePtr typ = nullptr;
  if (lhs->getType()->getUnbound()) {
    typ = expr->getType() ? expr->getType() : ctx->addUnbound(getSrcInfo());
  } else if (auto c = lhs->getType()->getClass()) {
    auto m = ctx->getRealizations()->findMethod(c->name, expr->member);
    if (m) {
      if (m->size() > 1)
        error("ambigious partial expression"); /// TODO
      if (lhs->isType()) {
        resultExpr = N<IdExpr>((*m)[0]->realizationInfo->name);
        resultExpr->setType(ctx->instantiate(getSrcInfo(), (*m)[0], c));
        return;
      } else { // cast y.foo to CLS.foo(y, ...)
        auto f = ctx->instantiate(getSrcInfo(), (*m)[0], c)->getFunc();
        vector<ExprPtr> args;
        args.push_back(move(lhs));
        for (int i = 0; i < f->args.size() - 2; i++)
          args.push_back(N<EllipsisExpr>());
        resultExpr = transform(
            N<CallExpr>(N<IdExpr>((*m)[0]->realizationInfo->name), move(args)));
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
  resultExpr = N<TypeOfExpr>(transform(expr->expr, true));
  resultExpr->markType();
  resultExpr->setType(forceUnify(expr, expr->expr->getType()));
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

  for (auto &p : params)
    DBG("param {}", p.name);
  string fnVar = getTemporaryVar("anonFn");
  prepend(N<FunctionStmt>(fnVar, nullptr, vector<Param>{}, move(params),
                          N<ReturnStmt>(expr->expr->clone()),
                          vector<string>{}));
  vector<CallExpr::Arg> args;
  for (int i = 0; i < expr->vars.size(); i++)
    args.push_back({"", N<EllipsisExpr>()});
  for (auto &c : cv.captures)
    if (used.find(c) == used.end())
      args.push_back({"", N<IdExpr>(c)});
  resultExpr = transform(N<CallExpr>(N<IdExpr>(fnVar), move(args)));
}

// TODO
void TransformVisitor::visit(const YieldExpr *expr) {
  error("yieldexpr is not yet supported");
}

} // namespace ast
} // namespace seq
