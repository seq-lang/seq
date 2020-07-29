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

class StaticVisitor : public WalkVisitor {
  std::shared_ptr<TypeContext> ctx;

public:
  std::unordered_map<string, TypePtr> statics;
  int value;

  using WalkVisitor::visit;
  StaticVisitor(std::shared_ptr<TypeContext> ctx);
  int transform(const Expr *e);
  void visit(const IdExpr *) override;
  void visit(const IntExpr *) override;
  void visit(const IfExpr *) override;
  void visit(const UnaryExpr *) override;
  void visit(const BinaryExpr *) override;
};

TransformVisitor::TransformVisitor(shared_ptr<TypeContext> ctx,
                                   shared_ptr<vector<StmtPtr>> stmts)
    : ctx(ctx) {
  prependStmts = stmts ? stmts : make_shared<vector<StmtPtr>>();
}

ExprPtr TransformVisitor::transform(const Expr *expr, bool allowTypes) {
  if (!expr)
    return nullptr;
  TransformVisitor v(ctx, prependStmts);
  v.setSrcInfo(expr->getSrcInfo());
  expr->accept(v);
  LOG9("[expr] {} -> {}", *expr, *v.resultExpr);
  if (v.resultExpr && v.resultExpr->getType() &&
      v.resultExpr->getType()->canRealize()) {
    if (auto c = v.resultExpr->getType()->getClass())
      realizeType(c);
  }
  if (ctx->isTypeChecking() && !allowTypes && v.resultExpr && v.resultExpr->isType())
    error("unexpected type expression");
  return move(v.resultExpr);
}

ExprPtr TransformVisitor::transformType(const ExprPtr &expr) {
  auto e = transform(expr.get(), true);
  if (ctx->isTypeChecking() && e && !e->isType())
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
  if (ctx->isTypeChecking())
    resultExpr->setType(forceUnify(resultExpr, ctx->findInternal("bool")));
}

void TransformVisitor::visit(const IntExpr *expr) {
  resultExpr = expr->clone();
  if (ctx->isTypeChecking())
    resultExpr->setType(forceUnify(resultExpr, ctx->findInternal("int")));
}

void TransformVisitor::visit(const FloatExpr *expr) {
  resultExpr = expr->clone();
  if (ctx->isTypeChecking())
    resultExpr->setType(forceUnify(resultExpr, ctx->findInternal("float")));
}

void TransformVisitor::visit(const StringExpr *expr) {
  resultExpr = expr->clone();
  if (ctx->isTypeChecking())
    resultExpr->setType(forceUnify(resultExpr, ctx->findInternal("str")));
}

// Transformed
void TransformVisitor::visit(const FStringExpr *expr) {
  int braceCount = 0, braceStart = 0;
  vector<ExprPtr> items;
  for (int i = 0; i < expr->value.size(); i++) {
    if (expr->value[i] == '{') {
      if (braceStart < i)
        items.push_back(N<StringExpr>(expr->value.substr(braceStart, i - braceStart)));
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
    items.push_back(
        N<StringExpr>(expr->value.substr(braceStart, expr->value.size() - braceStart)));
  resultExpr = transform(
      N<CallExpr>(N<DotExpr>(N<IdExpr>("str"), "cat"), N<ListExpr>(move(items))));
}

// Transformed
void TransformVisitor::visit(const KmerExpr *expr) {
  resultExpr = transform(
      N<CallExpr>(N<IndexExpr>(N<IdExpr>("Kmer"), N<IntExpr>(expr->value.size())),
                  N<SeqExpr>(expr->value)));
}

// Transformed
void TransformVisitor::visit(const SeqExpr *expr) {
  if (expr->prefix == "p")
    resultExpr = transform(N<CallExpr>(N<IdExpr>("pseq"), N<StringExpr>(expr->value)));
  else if (expr->prefix == "s")
    resultExpr = transform(N<CallExpr>(N<IdExpr>("seq"), N<StringExpr>(expr->value)));
  else
    error("invalid prefix '{}'", expr->prefix);
}

void TransformVisitor::visit(const IdExpr *expr) {
  resultExpr = expr->clone();
  auto val = processIdentifier(ctx, expr->value);
  if (!val)
    error("identifier '{}' not found", expr->value);
  if (val->getVar() &&
      (val->getModule() != ctx->getFilename() || val->getBase() != ctx->getBase()) &&
      val->getBase() == "") { // handle only toplevel calls
    // if (!val->getVar()->getType()->canRealize())
    //  error("the type of global variable '{}' is not realized", expr->value);
    resultExpr = transform(N<DotExpr>(N<IdExpr>("/" + val->getModule()), expr->value));
    return;
  }
  if (expr->value[0] != '.') {
    auto newName = expr->value;
    if (auto f = val->getFunc()) {
      newName = dynamic_pointer_cast<types::FuncType>(f->getType())->canonicalName;
    } else if (auto f = val->getClass()) {
      if (auto t = dynamic_pointer_cast<types::ClassType>(f->getType()))
        newName = t->name;
    }
    if (newName.size() && newName[0] == '.')
      resultExpr = N<IdExpr>(newName);
  }
  if (val->getClass() && !val->getClass()->getStatic())
    resultExpr->markType();
  if (ctx->isTypeChecking()) {
    if (val->getClass() && val->getClass()->getStatic()) {
      resultExpr = transform(N<IntExpr>(val->getType()->getStatic()->getValue()));
    } else {
      auto typ = val->getImport()
                     ? make_shared<types::ImportType>(val->getImport()->getFile())
                     : ctx->instantiate(getSrcInfo(), val->getType());
      resultExpr->setType(forceUnify(resultExpr, typ));
      auto newName = patchIfRealizable(typ, val->getClass());
      if (!newName.empty())
        static_cast<IdExpr *>(resultExpr.get())->value = newName;
    }
  }
}

// Transformed
void TransformVisitor::visit(const UnpackExpr *expr) {
  resultExpr = transform(N<CallExpr>(N<IdExpr>("list"), expr->what->clone()));
}

// Transformed
void TransformVisitor::visit(const TupleExpr *expr) {
  auto name = generateVariardicStub("tuple", expr->items.size());
  resultExpr = transform(
      N<CallExpr>(N<DotExpr>(N<IdExpr>(name), "__new__"), transform(expr->items)));
}

// Transformed
void TransformVisitor::visit(const ListExpr *expr) {
  string listVar = getTemporaryVar("lst");
  prepend(N<AssignStmt>(
      N<IdExpr>(listVar),
      N<CallExpr>(N<IdExpr>("list"),
                  expr->items.size() ? N<IntExpr>(expr->items.size()) : nullptr)));
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
    prepend(N<ExprStmt>(
        N<CallExpr>(N<DotExpr>(N<IdExpr>(setVar), "add"), expr->items[i]->clone())));
  resultExpr = transform(N<IdExpr>(setVar));
}

// Transformed
void TransformVisitor::visit(const DictExpr *expr) {
  string dictVar = getTemporaryVar("dict");
  prepend(N<AssignStmt>(N<IdExpr>(dictVar), N<CallExpr>(N<IdExpr>("dict"))));
  for (int i = 0; i < expr->items.size(); i++)
    prepend(N<ExprStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(dictVar), "__setitem__"),
                                    expr->items[i].key->clone(),
                                    expr->items[i].value->clone())));
  resultExpr = transform(N<IdExpr>(dictVar));
}

// Transformed
void TransformVisitor::visit(const GeneratorExpr *expr) {
  SuiteStmt *prev;
  auto suite = getGeneratorBlock(expr->loops, prev);
  string var = getTemporaryVar("gen");
  if (expr->kind == GeneratorExpr::ListGenerator) {
    prepend(N<AssignStmt>(N<IdExpr>(var), N<CallExpr>(N<IdExpr>("list"))));
    prev->stmts.push_back(N<ExprStmt>(
        N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "append"), expr->expr->clone())));
    prepend(move(suite));
  } else if (expr->kind == GeneratorExpr::SetGenerator) {
    prepend(N<AssignStmt>(N<IdExpr>(var), N<CallExpr>(N<IdExpr>("set"))));
    prev->stmts.push_back(N<ExprStmt>(
        N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "insert"), expr->expr->clone())));
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
      args.push_back({"", N<IdExpr>(c)});
    prepend(N<AssignStmt>(
        N<IdExpr>(var),
        N<CallExpr>(N<IdExpr>("iter"), N<CallExpr>(N<IdExpr>(fnVar), move(args)))));
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
  auto e =
      N<IfExpr>(makeBoolExpr(expr->cond), transform(expr->eif), transform(expr->eelse));
  if (ctx->isTypeChecking())
    e->setType(forceUnify(expr, e->eif->getType()));
  resultExpr = move(e);
}

// Transformed
void TransformVisitor::visit(const UnaryExpr *expr) {
  if (expr->op == "!") { // Special case
    auto e = N<UnaryExpr>(expr->op, makeBoolExpr(expr->expr));
    if (ctx->isTypeChecking())
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
      {"+", "add"},     {"-", "sub"},     {"*", "mul"}, {"**", "pow"}, {"/", "truediv"},
      {"//", "div"},    {"@", "mathmul"}, {"%", "mod"}, {"<", "lt"},   {"<=", "le"},
      {">", "gt"},      {">=", "ge"},     {"==", "eq"}, {"!=", "ne"},  {"<<", "lshift"},
      {">>", "rshift"}, {"&", "and"},     {"|", "or"},  {"^", "xor"}};
  if (expr->op == "&&" || expr->op == "||") { // Special case
    auto e =
        N<BinaryExpr>(makeBoolExpr(expr->lexpr), expr->op, makeBoolExpr(expr->rexpr));
    if (ctx->isTypeChecking())
      e->setType(forceUnify(expr, ctx->findInternal("bool")));
    resultExpr = move(e);
  } else if (expr->op == "is") {
    // TODO: check is Type or raw!
    // auto l = transform(expr->lexpr), r = transform(expr->rexpr);
    // if (r->isType()) {
    // }
    auto e = N<BinaryExpr>(transform(expr->lexpr), expr->op, transform(expr->rexpr));
    if (ctx->isTypeChecking())
      e->setType(forceUnify(expr, ctx->findInternal("bool")));
    resultExpr = move(e);
  } else if (expr->op == "is not") {
    resultExpr = transform(N<UnaryExpr>(
        "!", N<BinaryExpr>(expr->lexpr->clone(), "is", expr->rexpr->clone())));
  } else if (expr->op == "not in") {
    resultExpr = transform(N<UnaryExpr>(
        "!", N<BinaryExpr>(expr->lexpr->clone(), "in", expr->rexpr->clone())));
  } else if (expr->op == "in") {
    resultExpr = transform(N<CallExpr>(N<DotExpr>(expr->rexpr->clone(), "__contains__"),
                                       expr->lexpr->clone()));
  } else {
    auto le = transform(expr->lexpr);
    auto re = transform(expr->rexpr);
    if (!ctx->isTypeChecking()) {
      resultExpr = N<BinaryExpr>(move(le), expr->op, move(re));
    } else if (le->getType()->getUnbound() || re->getType()->getUnbound()) {
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
      } else if (findBestCall(rc, format("__r{}__", magic), {{"", rc}, {"", lc}})) {
        magic = "r" + magic;
      } else {
        error("cannot find magic '{}' for {}", magic, lc->toString());
      }
      magic = format("__{}__", magic);
      resultExpr = transform(
          N<CallExpr>(N<DotExpr>(expr->lexpr->clone(), magic), expr->rexpr->clone()));
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
  TypePtr inType = nullptr;
  if (ctx->isTypeChecking())
    inType = extractType(items.back().expr->getType());
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
    if (ctx->isTypeChecking()) {
      inType = updateType(inType, inTypePos, items.back().expr);
      if (i < expr->items.size() - 1)
        inType = extractType(inType);
    }
  }
  resultExpr = N<PipeExpr>(move(items));
  if (ctx->isTypeChecking())
    resultExpr->setType(forceUnify(expr, inType));
}

void TransformVisitor::visit(const IndexExpr *expr) {
  ExprPtr e = nullptr;
  if (auto i = CAST(expr->expr, IdExpr)) { // special case: tuples and functions
    if (i->value == "tuple" || i->value == "function") {
      auto t = CAST(expr->index, TupleExpr);
      auto name = generateVariardicStub(i->value, t ? t->items.size() : 1);
      e = transform(N<IdExpr>(name), true);
    }
  }
  if (!e)
    e = transform(expr->expr, true);
  if (!ctx->isTypeChecking()) {
    resultExpr = N<IndexExpr>(move(e), transform(expr->index));
    return;
  }

  vector<TypePtr> generics;
  auto parseGeneric = [&](const ExprPtr &i) {
    auto ti = transform(i, true);
    if (ti->isType())
      generics.push_back(ti->getType());
    else
      try {
        StaticVisitor sv(ctx);
        i->accept(sv);
        generics.push_back(make_shared<StaticType>(sv.value));
      } catch (exc::ParserException &) {
        error(ti, "expected a type or a static expression");
      }
  };

  // Type or function realization (e.g. dict[type1, type2])
  if (e->isType() || (e->getType()->getFunc())) {
    if (auto t = CAST(expr->index, TupleExpr))
      for (auto &i : t->items)
        parseGeneric(i);
    else
      parseGeneric(expr->index);
    auto g = e->getType()->getClass();
    if (g->explicits.size() != generics.size())
      error("expected {} generics, got {}", g->explicits.size(), generics.size());
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
          resultExpr =
              transform(N<TupleIndexExpr>(expr->expr->clone(), std::stoll(ii->value)));
          return;
        }
      }
    resultExpr = transform(N<CallExpr>(N<DotExpr>(expr->expr->clone(), "__getitem__"),
                                       expr->index->clone()));
  }
}

void TransformVisitor::visit(const TupleIndexExpr *expr) {
  assert(ctx->isTypeChecking());

  auto e = transform(expr->expr);
  auto c = e->getType()->getClass();
  assert(chop(c->name).substr(0, 8) == "__tuple_");
  if (expr->index < 0 || expr->index >= c->args.size())
    error("tuple index out of range (expected 0..{}, got {})", c->args.size() - 1,
          expr->index);
  resultExpr = N<TupleIndexExpr>(move(e), expr->index);
  resultExpr->setType(forceUnify(expr, c->args[expr->index]));
}

void TransformVisitor::visit(const StackAllocExpr *expr) {
  auto te = transformType(expr->typeExpr);
  auto e = transform(expr->expr);

  auto t = te->getType();
  resultExpr = N<StackAllocExpr>(move(te), move(e));
  if (ctx->isTypeChecking()) {
    t = ctx->instantiateGeneric(expr->getSrcInfo(), ctx->findInternal("array"), {t});
    patchIfRealizable(t, true);
    resultExpr->setType(forceUnify(expr, t));
  }
}

void TransformVisitor::visit(const CallExpr *expr) {
  if (handleStackAlloc(expr))
    return;

  ExprPtr e = nullptr;
  vector<CallExpr::Arg> args;
  for (auto &i : expr->args)
    args.push_back({i.name, transform(i.value)});

  if (!ctx->isTypeChecking()) {
    resultExpr = N<CallExpr>(transform(expr->expr, true), move(args));
    return;
  }

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
      resultExpr = transform(N<CallExpr>(N<DotExpr>(move(e), "__new__"), move(args)));
    } else {
      string var = getTemporaryVar("typ");
      /// TODO: assumes that a class cannot have multiple __new__ magics
      /// WARN: passing e & args that have already been transformed
      prepend(
          N<AssignStmt>(N<IdExpr>(var), N<CallExpr>(N<DotExpr>(move(e), "__new__"))));
      prepend(
          N<ExprStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "__init__"), move(args))));
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
    resultExpr = transform(N<CallExpr>(N<DotExpr>(move(e), "__call__"), move(args)));
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
    if (!s.size() || s[0] != '/') {
      auto val = ctx->find(s);
      if (val && val->getImport())
        s = "/" + val->getImport()->getFile();
    }
    if (s.size() && s[0] == '/') {
      auto ictx = ctx->getImports()->getImport(s.substr(1))->tctx;
      auto ival = processIdentifier(ictx, expr->member);
      if (!ival)
        error("identifier '{}' not found in {}", expr->member, s.substr(1));
      resultExpr = N<DotExpr>(N<IdExpr>(s), expr->member);
      if (ctx->isTypeChecking()) {
        if (ival->getClass())
          resultExpr->markType();
        resultExpr->setType(
            forceUnify(expr, ctx->instantiate(getSrcInfo(), ival->getType())));
        auto newName = patchIfRealizable(resultExpr->getType(), ival->getClass());
        if (!newName.empty())
          static_cast<DotExpr *>(resultExpr.get())->member = newName;
      }
      return;
    }
  }

  auto lhs = transform(expr->expr, true);
  TypePtr typ = nullptr;
  if (ctx->isTypeChecking()) {
    if (lhs->getType()->getUnbound()) {
      typ = expr->getType() ? expr->getType() : ctx->addUnbound(getSrcInfo());
    } else if (auto c = lhs->getType()->getClass()) {
      if (auto m = ctx->getRealizations()->findMethod(c->name, expr->member)) {
        if (m->size() > 1)
          error("ambigious partial expression"); /// TODO
        if (lhs->isType()) {
          auto name = (*m)[0]->canonicalName;
          auto val = processIdentifier(ctx, name);
          assert(val);
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
          resultExpr =
              transform(N<CallExpr>(N<IdExpr>((*m)[0]->canonicalName), move(args)));
          return;
        }
      } else if (auto mm = ctx->getRealizations()->findMember(c->name, expr->member)) {
        typ = ctx->instantiate(getSrcInfo(), mm, c);
      } else {
        error("cannot find '{}' in {}", expr->member, lhs->getType()->toString());
      }
    } else {
      error("cannot find '{}' in {}", expr->member, lhs->getType()->toString());
    }
  }
  resultExpr = N<DotExpr>(move(lhs), expr->member);
  if (ctx->isTypeChecking())
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
  if (ctx->isTypeChecking())
    resultExpr->setType(ctx->addUnbound(getSrcInfo()));
}

// Should get transformed by other functions
void TransformVisitor::visit(const TypeOfExpr *expr) {
  auto e = transform(expr->expr);
  if (ctx->isTypeChecking()) {
    auto t = forceUnify(expr, e->getType());

    auto newName = patchIfRealizable(t, true);
    if (!newName.empty())
      resultExpr = N<IdExpr>(newName);
    else
      resultExpr = N<TypeOfExpr>(move(e));
    resultExpr->markType();
    resultExpr->setType(t);
  } else {
    resultExpr = N<TypeOfExpr>(move(e));
  }
}

void TransformVisitor::visit(const PtrExpr *expr) {
  // TODO: force only variables here!
  auto param = transform(expr->expr);
  auto t = param->getType();
  resultExpr = N<PtrExpr>(move(param));
  if (ctx->isTypeChecking())
    resultExpr->setType(
        forceUnify(expr, ctx->instantiateGeneric(expr->getSrcInfo(),
                                                 ctx->findInternal("ptr"), {t})));
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
                          N<ReturnStmt>(expr->expr->clone()), vector<string>{}));
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

/*******************************/

template <typename... TArgs>
void error(const SrcInfo &s, const char *format, TArgs &&... args) {
  ast::error(s, fmt::format(format, args...).c_str());
}

// Transformed
CaptureVisitor::CaptureVisitor(shared_ptr<TypeContext> ctx) : ctx(ctx) {}

void CaptureVisitor::visit(const IdExpr *expr) {
  auto val = ctx->find(expr->value);
  if (val && val->getVar())
    captures.insert(expr->value);
}

StaticVisitor::StaticVisitor(std::shared_ptr<TypeContext> ctx) : ctx(ctx), value(0) {}

int StaticVisitor::transform(const Expr *e) {
  StaticVisitor v(ctx);
  e->accept(v);
  for (auto &i : v.statics)
    statics[i.first] = i.second;
  return v.value;
}

void StaticVisitor::visit(const IdExpr *expr) {
  auto val = ctx->find(expr->value);
  if (!val || !val->getClass() || !val->getClass()->getStatic())
    error(expr->getSrcInfo(), "identifier '{}' is not a static expression",
          expr->value);
  auto t = dynamic_pointer_cast<StaticType>(val->getClass()->getType());
  assert(t);
  if (!t->canRealize())
    statics[expr->value] = t;
  else
    value = t->getValue();
}

void StaticVisitor::visit(const IntExpr *expr) {
  if (expr->suffix.size())
    error(expr->getSrcInfo(), "not a static expression");
  try {
    value = std::stoull(expr->value, nullptr, 0);
  } catch (std::out_of_range &) {
    error(expr->getSrcInfo(), "integer {} out of range", expr->value);
  }
}

void StaticVisitor::visit(const UnaryExpr *expr) {
  value = transform(expr->expr.get());
  if (expr->op == "-")
    value = -value;
  else if (expr->op == "!")
    value = !bool(value);
  else
    error(expr->getSrcInfo(), "not a static unary expression");
}

void StaticVisitor::visit(const IfExpr *expr) {
  if (transform(expr->cond.get()))
    value = transform(expr->eif.get());
  else
    value = transform(expr->eelse.get());
}

void StaticVisitor::visit(const BinaryExpr *expr) {
  int lhs = transform(expr->lexpr.get());
  int rhs = transform(expr->rexpr.get());
  if (expr->op == "<")
    value = lhs < rhs;
  else if (expr->op == "<=")
    value = lhs <= rhs;
  else if (expr->op == ">")
    value = lhs > rhs;
  else if (expr->op == ">=")
    value = lhs >= rhs;
  else if (expr->op == "==")
    value = lhs == rhs;
  else if (expr->op == "!=")
    value = lhs != rhs;
  else if (expr->op == "&&")
    value = lhs && rhs;
  else if (expr->op == "||")
    value = lhs || rhs;
  else if (expr->op == "+")
    value = lhs + rhs;
  else if (expr->op == "-")
    value = lhs - rhs;
  else if (expr->op == "*")
    value = lhs * rhs;
  else if (expr->op == "//")
    value = lhs / rhs;
  else if (expr->op == "%")
    value = lhs % rhs;
  else
    error(expr->getSrcInfo(), "not a static binary expression");
}

} // namespace ast
} // namespace seq
