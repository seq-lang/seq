/*
 * simplify_expr.cpp --- AST expression simplifications.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */
#include <deque>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "parser/ast.h"
#include "parser/cache.h"
#include "parser/common.h"
#include "parser/ocaml/ocaml.h"
#include "parser/visitors/simplify/simplify.h"

using fmt::format;

namespace seq {
namespace ast {

ExprPtr SimplifyVisitor::transform(const ExprPtr &expr) {
  return transform(expr.get(), false);
}

ExprPtr SimplifyVisitor::transform(const Expr *expr, bool allowTypes) {
  if (!expr)
    return nullptr;
  SimplifyVisitor v(ctx, preamble);
  v.setSrcInfo(expr->getSrcInfo());
  const_cast<Expr *>(expr)->accept(v);
  if (!allowTypes && v.resultExpr && v.resultExpr->isType())
    error("unexpected type expression");
  return move(v.resultExpr);
}

ExprPtr SimplifyVisitor::transformType(const Expr *expr) {
  auto e = transform(expr, true);
  if (e && !e->isType())
    error("expected type expression");
  return e;
}

void SimplifyVisitor::defaultVisit(Expr *e) { resultExpr = e->clone(); }

/**************************************************************************************/

void SimplifyVisitor::visit(NoneExpr *expr) {
  resultExpr = transform(N<CallExpr>(N<IdExpr>(TYPE_OPTIONAL)));
}

void SimplifyVisitor::visit(IntExpr *expr) {
  resultExpr = transformInt(expr->value, expr->suffix);
}

void SimplifyVisitor::visit(StringExpr *expr) {
  if (expr->prefix == "f") {
    /// F-strings
    resultExpr = transformFString(expr->value);
  } else if (!expr->prefix.empty()) {
    /// Custom-prefix strings
    resultExpr = transform(
        N<CallExpr>(N<IndexExpr>(N<DotExpr>(N<IdExpr>("str"),
                                            format("__prefix_{}__", expr->prefix)),
                                 N<IntExpr>(expr->value.size())),
                    N<StringExpr>(expr->value)));
  } else {
    resultExpr = expr->clone();
  }
}

void SimplifyVisitor::visit(IdExpr *expr) {
  auto val = ctx->find(expr->value);
  if (!val)
    error("identifier '{}' not found", expr->value);

  // If we are accessing an outer non-global variable, raise an error unless
  // we are capturing variables (in that case capture it).
  bool captured = false;
  auto newName = val->canonicalName;
  if (val->isVar()) {
    if (ctx->getBase() != val->getBase() && !val->isGlobal()) {
      if (!ctx->captures.empty()) {
        captured = true;
        if (!in(ctx->captures.back(), val->canonicalName)) {
          ctx->captures.back()[val->canonicalName] = newName =
              ctx->generateCanonicalName(val->canonicalName);
          ctx->cache->reverseIdentifierLookup[newName] = newName;
        }
        newName = ctx->captures.back()[val->canonicalName];
      } else {
        error("cannot access non-global variable '{}'",
              ctx->cache->reverseIdentifierLookup[expr->value]);
      }
    }
  }

  // Replace the variable with its canonical name. Do not canonize captured
  // variables (they will be later passed as argument names).
  resultExpr = N<IdExpr>(newName);
  // Flag the expression as a type expression if it points to a class name or a generic.
  if (val->isType() && !val->isStatic())
    resultExpr->markType();
  if (val->isStatic())
    resultExpr->isStaticExpr = true;

  // The only variables coming from the enclosing base must be class generics.
  seqassert(!val->isFunc() || val->getBase().empty(), "{} has invalid base ({})",
            expr->value, val->getBase());
  if (val->isType() && !val->getBase().empty() && ctx->getBase() != val->getBase()) {
    if (ctx->bases.size() == 2 && ctx->bases[0].name == val->getBase()) {
      ctx->bases.back().attributes |= FLAG_METHOD;
      return;
    }
  }
  // If that is not the case, we are probably having a class accessing its enclosing
  // function variable (generic or other identifier). We do not like that!
  if (!captured && ctx->getBase() != val->getBase() && !val->getBase().empty())
    error("identifier '{}' not found (cannot access outer function identifiers)",
          expr->value);
}

void SimplifyVisitor::visit(StarExpr *expr) {
  error("cannot use star-expression here");
}

void SimplifyVisitor::visit(TupleExpr *expr) {
  vector<ExprPtr> items;
  for (auto &i : expr->items) {
    if (auto es = i->getStar())
      items.emplace_back(N<StarExpr>(transform(es->what)));
    else
      items.emplace_back(transform(i));
  }
  resultExpr = N<TupleExpr>(move(items));
}

void SimplifyVisitor::visit(ListExpr *expr) {
  vector<StmtPtr> stmts;
  ExprPtr var = N<IdExpr>(ctx->cache->getTemporaryVar("list"));
  stmts.push_back(transform(N<AssignStmt>(
      clone(var),
      N<CallExpr>(N<IdExpr>("List"),
                  !expr->items.empty() ? N<IntExpr>(expr->items.size()) : nullptr),
      nullptr, true)));
  for (const auto &it : expr->items) {
    if (auto star = it->getStar()) {
      ExprPtr forVar = N<IdExpr>(ctx->cache->getTemporaryVar("it"));
      stmts.push_back(transform(N<ForStmt>(
          clone(forVar), star->what->clone(),
          N<ExprStmt>(N<CallExpr>(N<DotExpr>(clone(var), "append"), clone(forVar))))));
    } else {
      stmts.push_back(transform(
          N<ExprStmt>(N<CallExpr>(N<DotExpr>(clone(var), "append"), clone(it)))));
    }
  }
  resultExpr = N<StmtExpr>(move(stmts), transform(var));
}

void SimplifyVisitor::visit(SetExpr *expr) {
  vector<StmtPtr> stmts;
  ExprPtr var = N<IdExpr>(ctx->cache->getTemporaryVar("set"));
  stmts.push_back(transform(
      N<AssignStmt>(clone(var), N<CallExpr>(N<IdExpr>("Set")), nullptr, true)));
  for (auto &it : expr->items)
    if (auto star = it->getStar()) {
      ExprPtr forVar = N<IdExpr>(ctx->cache->getTemporaryVar("it"));
      stmts.push_back(transform(N<ForStmt>(
          clone(forVar), star->what->clone(),
          N<ExprStmt>(N<CallExpr>(N<DotExpr>(clone(var), "add"), clone(forVar))))));
    } else {
      stmts.push_back(transform(
          N<ExprStmt>(N<CallExpr>(N<DotExpr>(clone(var), "add"), clone(it)))));
    }
  resultExpr = N<StmtExpr>(move(stmts), transform(var));
}

void SimplifyVisitor::visit(DictExpr *expr) {
  vector<StmtPtr> stmts;
  ExprPtr var = N<IdExpr>(ctx->cache->getTemporaryVar("dict"));
  stmts.push_back(transform(
      N<AssignStmt>(clone(var), N<CallExpr>(N<IdExpr>("Dict")), nullptr, true)));
  for (auto &it : expr->items)
    if (auto star = CAST(it.value, KeywordStarExpr)) {
      ExprPtr forVar = N<IdExpr>(ctx->cache->getTemporaryVar("it"));
      stmts.push_back(transform(N<ForStmt>(
          clone(forVar), N<CallExpr>(N<DotExpr>(star->what->clone(), "items")),
          N<ExprStmt>(N<CallExpr>(N<DotExpr>(clone(var), "__setitem__"),
                                  N<IndexExpr>(clone(forVar), N<IntExpr>(0)),
                                  N<IndexExpr>(clone(forVar), N<IntExpr>(1)))))));
    } else {
      stmts.push_back(transform(N<ExprStmt>(N<CallExpr>(
          N<DotExpr>(clone(var), "__setitem__"), clone(it.key), clone(it.value)))));
    }
  resultExpr = N<StmtExpr>(move(stmts), transform(var));
}

void SimplifyVisitor::visit(GeneratorExpr *expr) {
  SuiteStmt *prev;
  vector<StmtPtr> stmts;

  auto loops = clone_nop(expr->loops);
  // List comprehension optimization: pass iter.__len__() if we only have a single for
  // loop without any conditions.
  string optimizeVar;
  if (expr->kind == GeneratorExpr::ListGenerator && loops.size() == 1 &&
      loops[0].conds.empty()) {
    optimizeVar = ctx->cache->getTemporaryVar("iter");
    stmts.push_back(transform(
        N<AssignStmt>(N<IdExpr>(optimizeVar), move(loops[0].gen), nullptr, true)));
    loops[0].gen = N<IdExpr>(optimizeVar);
  }

  auto suite = transformGeneratorBody(loops, prev);
  ExprPtr var = N<IdExpr>(ctx->cache->getTemporaryVar("gen"));
  if (expr->kind == GeneratorExpr::ListGenerator) {
    vector<CallExpr::Arg> args;
    if (!optimizeVar.empty()) {
      // Use special List.__init__(bool, T) constructor.
      args.emplace_back(CallExpr::Arg{"", N<BoolExpr>(true)});
      args.emplace_back(CallExpr::Arg{"", N<IdExpr>(optimizeVar)});
    }
    stmts.push_back(transform(N<AssignStmt>(
        clone(var), N<CallExpr>(N<IdExpr>("List"), move(args)), nullptr, true)));
    prev->stmts.push_back(
        N<ExprStmt>(N<CallExpr>(N<DotExpr>(clone(var), "append"), clone(expr->expr))));
    stmts.push_back(transform(suite));
    resultExpr = N<StmtExpr>(move(stmts), transform(var));
  } else if (expr->kind == GeneratorExpr::SetGenerator) {
    stmts.push_back(transform(
        N<AssignStmt>(clone(var), N<CallExpr>(N<IdExpr>("Set")), nullptr, true)));
    prev->stmts.push_back(
        N<ExprStmt>(N<CallExpr>(N<DotExpr>(clone(var), "add"), clone(expr->expr))));
    stmts.push_back(transform(suite));
    resultExpr = N<StmtExpr>(move(stmts), transform(var));
  } else {
    prev->stmts.push_back(N<YieldStmt>(clone(expr->expr)));
    stmts.push_back(move(suite));
    resultExpr =
        N<CallExpr>(N<DotExpr>(N<CallExpr>(makeAnonFn(move(stmts))), "__iter__"));
  }
}

void SimplifyVisitor::visit(DictGeneratorExpr *expr) {
  SuiteStmt *prev;
  auto suite = transformGeneratorBody(expr->loops, prev);

  vector<StmtPtr> stmts;
  ExprPtr var = N<IdExpr>(ctx->cache->getTemporaryVar("gen"));
  stmts.push_back(transform(
      N<AssignStmt>(clone(var), N<CallExpr>(N<IdExpr>("Dict")), nullptr, true)));
  prev->stmts.push_back(N<ExprStmt>(N<CallExpr>(N<DotExpr>(clone(var), "__setitem__"),
                                                clone(expr->key), clone(expr->expr))));
  stmts.push_back(transform(suite));
  resultExpr = N<StmtExpr>(move(stmts), transform(var));
}

void SimplifyVisitor::visit(IfExpr *expr) {
  auto cond = transform(expr->cond);
  auto oldAssign = ctx->canAssign;
  ctx->canAssign = false;
  auto newExpr =
      N<IfExpr>(move(cond), transform(expr->ifexpr), transform(expr->elsexpr));
  ctx->canAssign = oldAssign;
  newExpr->isStaticExpr = newExpr->cond->isStaticExpr &&
                          newExpr->ifexpr->isStaticExpr &&
                          newExpr->elsexpr->isStaticExpr;
  resultExpr = move(newExpr);
}

void SimplifyVisitor::visit(UnaryExpr *expr) {
  auto newExpr = transform(expr->expr);
  if (newExpr->isStaticExpr && (expr->op == "!" || expr->op == "-")) {
    resultExpr = N<UnaryExpr>(expr->op, move(newExpr));
    resultExpr->isStaticExpr = true;
  } else if (expr->op == "!") {
    resultExpr = transform(N<CallExpr>(N<DotExpr>(
        N<CallExpr>(N<DotExpr>(clone(expr->expr), "__bool__")), "__invert__")));
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
    resultExpr = transform(N<CallExpr>(N<DotExpr>(clone(expr->expr), magic)));
  }
}

void SimplifyVisitor::visit(BinaryExpr *expr) {
  static unordered_set<string> supportedStaticOp{
      "<", "<=", ">", ">=", "==", "!=", "&&", "||", "+", "-", "*", "//", "%"};
  auto lhs = transform(expr->lexpr);
  auto oldAssign = ctx->canAssign;
  if (expr->op == "&&" || expr->op == "||")
    ctx->canAssign = false;
  auto rhs = transform(expr->rexpr);
  if (expr->op == "&&" || expr->op == "||")
    ctx->canAssign = oldAssign;
  if (lhs->isStaticExpr && rhs->isStaticExpr && in(supportedStaticOp, expr->op) &&
      !expr->inPlace) {
    resultExpr = N<BinaryExpr>(move(lhs), expr->op, move(rhs));
    resultExpr->isStaticExpr = true;
  } else if (expr->op == "&&") {
    resultExpr =
        N<IfExpr>(N<CallExpr>(N<DotExpr>(move(lhs), "__bool__")),
                  N<CallExpr>(N<DotExpr>(move(rhs), "__bool__")), N<BoolExpr>(false));
  } else if (expr->op == "||") {
    resultExpr =
        N<IfExpr>(N<CallExpr>(N<DotExpr>(move(lhs), "__bool__")), N<BoolExpr>(true),
                  N<CallExpr>(N<DotExpr>(move(rhs), "__bool__")));
  } else if (expr->op == "not in") {
    resultExpr = N<CallExpr>(N<DotExpr>(
        N<CallExpr>(N<DotExpr>(move(rhs), "__contains__"), move(lhs)), "__invert__"));
  } else if (expr->op == "in") {
    resultExpr = N<CallExpr>(N<DotExpr>(move(rhs), "__contains__"), move(lhs));
  } else if (expr->op == "is" || expr->op == "is not") {
    auto le = expr->lexpr->getNone() ? clone(expr->lexpr) : move(lhs);
    auto re = expr->rexpr->getNone() ? clone(expr->rexpr) : move(rhs);
    if (expr->lexpr->getNone() && expr->rexpr->getNone())
      resultExpr = N<BoolExpr>(true);
    else if (expr->lexpr->getNone())
      resultExpr = N<BinaryExpr>(move(re), "is", move(le));
    else
      resultExpr = N<BinaryExpr>(move(le), "is", move(re));
    if (expr->op == "is not")
      resultExpr = N<CallExpr>(N<DotExpr>(move(resultExpr), "__invert__"));
  } else {
    resultExpr = N<BinaryExpr>(move(lhs), expr->op, move(rhs), expr->inPlace);
  }
}

void SimplifyVisitor::visit(ChainBinaryExpr *expr) {
  seqassert(expr->exprs.size() >= 2, "not enough expressions in ChainBinaryExpr");
  vector<ExprPtr> e;
  string prev;
  for (int i = 1; i < expr->exprs.size(); i++) {
    auto l = prev.empty() ? clone(expr->exprs[i - 1].second) : N<IdExpr>(prev);
    prev = ctx->generateCanonicalName("chain");
    auto r =
        (i + 1 == expr->exprs.size())
            ? clone(expr->exprs[i].second)
            : N<StmtExpr>(N<AssignStmt>(N<IdExpr>(prev), clone(expr->exprs[i].second)),
                          N<IdExpr>(prev));
    e.emplace_back(N<BinaryExpr>(move(l), expr->exprs[i].first, move(r)));
  }

  int i = int(e.size()) - 1;
  ExprPtr b = move(e[i]);
  for (i -= 1; i >= 0; i--)
    b = N<BinaryExpr>(move(e[i]), "&&", move(b));
  resultExpr = transform(b);
}

void SimplifyVisitor::visit(PipeExpr *expr) {
  vector<PipeExpr::Pipe> p;
  for (auto &i : expr->items) {
    auto e = clone(i.expr);
    if (auto ec = const_cast<CallExpr *>(e->getCall())) {
      for (auto &a : ec->args)
        if (auto ee = const_cast<EllipsisExpr *>(a.value->getEllipsis()))
          ee->isPipeArg = true;
    }
    p.push_back({i.op, transform(e)});
  }
  resultExpr = N<PipeExpr>(move(p));
}

void SimplifyVisitor::visit(IndexExpr *expr) {
  ExprPtr e = nullptr;
  auto index = expr->index->clone();
  // First handle the tuple[] and function[] cases.
  if (expr->expr->isId("tuple") || expr->expr->isId("Tuple")) {
    auto t = index->getTuple();
    e = N<IdExpr>(format(TYPE_TUPLE "{}", t ? t->items.size() : 1));
    e->markType();
  } else if (expr->expr->isId("function") || expr->expr->isId("Function") ||
             expr->expr->isId("Callable")) {
    auto t = const_cast<TupleExpr *>(index->getTuple());
    if (t->items.size() != 2 || !t->items[0]->getList())
      error("invalid {} type declaration", expr->expr->getId()->value);
    for (auto &i : const_cast<ListExpr *>(t->items[0]->getList())->items)
      t->items.emplace_back(move(i));
    t->items.erase(t->items.begin());
    e = N<IdExpr>(
        format(expr->expr->isId("Callable") ? TYPE_CALLABLE "{}" : TYPE_FUNCTION "{}",
               t ? int(t->items.size()) - 1 : 0));
    e->markType();
  } else {
    e = transform(expr->expr.get(), true);
  }
  // IndexExpr[i1, ..., iN] is internally stored as IndexExpr[TupleExpr[i1, ..., iN]]
  // for N > 1, so make sure to check that case.
  vector<ExprPtr> it;
  if (auto t = index->getTuple())
    for (auto &i : t->items)
      it.push_back(transform(i.get(), true));
  else
    it.push_back(transform(index.get(), true));

  // Below we check if this is a proper instantiation expression.
  bool allTypes = true;
  bool hasRealTypes = false; // real types are non-static type expressions
  for (auto &i : it) {
    if (i->isType())
      hasRealTypes = true;
    if (!i->isType() && !i->isStaticExpr)
      allTypes = false;
    if (i->isType() && !allTypes)
      error(i, "invalid type expression");
  }
  if (!allTypes && e->isType())
    error("expected type parameters");
  if (allTypes && e->isType()) {
    resultExpr = N<InstantiateExpr>(move(e), move(it));
    resultExpr->markType();
  } else if (allTypes && hasRealTypes) {
    resultExpr = N<InstantiateExpr>(move(e), move(it));
  } else {
    // For some expressions (e.g. self.foo[N]) we are not yet sure if it is an
    // instantiation or an element access (the expression might be a function and we
    // do not know it yet, and all indices are StaticExpr).
    resultExpr =
        N<IndexExpr>(move(e), it.size() == 1 ? move(it[0]) : N<TupleExpr>(move(it)));
  }
}

void SimplifyVisitor::visit(CallExpr *expr) {
  // Special calls
  // 1. __ptr__(v)
  if (expr->expr->isId("__ptr__")) {
    if (expr->args.size() == 1 && expr->args[0].value->getId()) {
      auto v = ctx->find(expr->args[0].value->getId()->value);
      if (v && v->isVar()) {
        resultExpr = N<PtrExpr>(transform(expr->args[0].value));
        return;
      }
    }
    error("__ptr__ only accepts a single argument (variable identifier)");
  }
  // 2. __array__[T](n)
  if (expr->expr->getIndex() && expr->expr->getIndex()->expr->isId("__array__")) {
    if (expr->args.size() != 1)
      error("__array__ only accepts a single argument (size)");
    resultExpr = N<StackAllocExpr>(transformType(expr->expr->getIndex()->index.get()),
                                   transform(expr->args[0].value));
    return;
  }
  // 3. isinstance(v, T)
  if (expr->expr->isId("isinstance")) {
    if (expr->args.size() != 2 || !expr->args[0].name.empty() ||
        !expr->args[1].name.empty())
      error("isinstance only accepts two arguments");
    auto lhs = transform(expr->args[0].value.get(), true);
    ExprPtr type;
    if (expr->args[1].value->isId("Tuple") || expr->args[1].value->isId("tuple") ||
        (lhs->isType() && expr->args[1].value->getNone()))
      type = expr->args[1].value->clone();
    else
      type = transformType(expr->args[1].value.get());
    resultExpr = N<CallExpr>(clone(expr->expr), move(lhs), move(type));
    resultExpr->isStaticExpr = true;
    return;
  }
  // 4. staticlen(v)
  if (expr->expr->isId("staticlen")) {
    if (expr->args.size() != 1)
      error("staticlen only accepts a single arguments");
    resultExpr = N<CallExpr>(clone(expr->expr), transform(expr->args[0].value));
    resultExpr->isStaticExpr = true;
    return;
  }
  // 5. hasattr(v, "id")
  if (expr->expr->isId("hasattr")) {
    if (expr->args.size() != 2 || !expr->args[0].name.empty() ||
        !expr->args[1].name.empty())
      error("hasattr accepts two arguments");
    auto s = transform(expr->args[1].value);
    if (!s->getString())
      error("hasattr requires the second string to be a compile-time string");
    resultExpr = N<CallExpr>(clone(expr->expr),
                             transformType(expr->args[0].value.get()), move(s));
    resultExpr->isStaticExpr = true;
    return;
  }
  // 6. compile_error("msg")
  if (expr->expr->isId("compile_error")) {
    if (expr->args.size() != 1)
      error("compile_error accepts a single argument");
    auto s = transform(expr->args[0].value);
    if (!s->getString())
      error("compile_error requires the second string to be a compile-time string");
    resultExpr = N<CallExpr>(clone(expr->expr), move(s));
    return;
  }
  // 7. tuple(i for i in j)
  if (expr->expr->isId("tuple")) {
    GeneratorExpr *g = nullptr;
    if (expr->args.size() != 1 || !(g = CAST(expr->args[0].value, GeneratorExpr)) ||
        g->kind != GeneratorExpr::Generator || g->loops.size() != 1 ||
        !g->loops[0].conds.empty())
      error("tuple only accepts a simple comprehension over a tuple");

    ctx->addBlock();
    auto var = clone(g->loops[0].vars);
    auto ex = clone(g->expr);
    if (auto i = var->getId()) {
      ctx->add(SimplifyItem::Var, i->value, ctx->generateCanonicalName(i->value));
      var = transform(var);
      ex = transform(ex);
    } else {
      string varName = ctx->cache->getTemporaryVar("for");
      ctx->add(SimplifyItem::Var, varName, varName);
      var = N<IdExpr>(varName);
      ex = N<StmtExpr>(
          transform(N<AssignStmt>(clone(g->loops[0].vars), clone(var), nullptr, true)),
          transform(ex));
    }
    vector<GeneratorBody> body;
    body.push_back({move(var), transform(g->loops[0].gen), {}});
    resultExpr = N<GeneratorExpr>(GeneratorExpr::Generator, move(ex), move(body));
    ctx->popBlock();
    return;
  }

  auto e = transform(expr->expr.get(), true);
  // 8. namedtuple
  if (e->isId("std.collections.namedtuple")) {
    if (expr->args.size() != 2 || !expr->args[0].value->getString() ||
        !expr->args[1].value->getList())
      error("invalid namedtuple arguments");
    vector<Param> generics, args;
    int ti = 1;
    for (auto &i : expr->args[1].value->getList()->items)
      if (auto s = i->getString()) {
        generics.emplace_back(Param{format("T{}", ti), nullptr, nullptr});
        args.emplace_back(Param{s->value, N<IdExpr>(format("T{}", ti++)), nullptr});
      } else if (i->getTuple() && i->getTuple()->items.size() == 2 &&
                 i->getTuple()->items[0]->getString()) {
        args.emplace_back(Param{i->getTuple()->items[0]->getString()->value,
                                transformType(i->getTuple()->items[1].get()), nullptr});
      } else {
        error("invalid namedtuple arguments");
      }
    auto name = expr->args[0].value->getString()->value;
    transform(
        N<ClassStmt>(name, move(generics), move(args), nullptr, Attr({Attr::Tuple})));
    auto i = N<IdExpr>(name);
    resultExpr = transformType(i.get());
    return;
  }
  // 9. partial
  if (e->isId("std.functools.partial")) {
    if (expr->args.size() < 1)
      error("invalid namedtuple arguments");
    vector<CallExpr::Arg> args = clone_nop(expr->args);
    args.erase(args.begin());
    args.push_back({"", N<EllipsisExpr>()});
    resultExpr = transform(N<CallExpr>(clone(expr->args[0].value), move(args)));
    return;
  }

  vector<CallExpr::Arg> args;
  bool namesStarted = false;
  bool foundEllispis = false;
  for (auto &i : expr->args) {
    if (i.name.empty() && namesStarted &&
        !(CAST(i.value, KeywordStarExpr) || i.value->getEllipsis()))
      error("unnamed argument after a named argument");
    if (!i.name.empty() && (i.value->getStar() || CAST(i.value, KeywordStarExpr)))
      error("named star-expressions not allowed");
    namesStarted |= !i.name.empty();
    if (auto ee = i.value->getEllipsis()) {
      if (foundEllispis ||
          (!ee->isPipeArg && i.value.get() != expr->args.back().value.get()))
        error("unexpected ellipsis expression");
      foundEllispis = true;
      args.push_back({i.name, clone(i.value)});
    } else if (auto es = i.value->getStar())
      args.push_back({i.name, N<StarExpr>(transform(es->what))});
    else if (auto ek = CAST(i.value, KeywordStarExpr))
      args.push_back({i.name, N<KeywordStarExpr>(transform(ek->what))});
    else
      args.push_back({i.name, transform(i.value)});
  }
  resultExpr = N<CallExpr>(move(e), move(args));
}

void SimplifyVisitor::visit(DotExpr *expr) {
  /// First flatten the imports.
  const Expr *e = expr;
  std::deque<string> chain;
  while (auto d = e->getDot()) {
    chain.push_front(d->member);
    e = d->expr.get();
  }
  if (auto d = e->getId()) {
    chain.push_front(d->value);

    /// Check if this is a import or a class access:
    /// (import1.import2...).(class1.class2...)?.method?
    int importEnd = 0, itemEnd = 0;
    string importName, itemName;
    shared_ptr<SimplifyItem> val = nullptr;
    for (int i = int(chain.size()) - 1; i >= 0; i--) {
      auto s = join(chain, "/", 0, i + 1);
      val = ctx->find(s);
      if (val && val->isImport()) {
        importName = val->canonicalName;
        importEnd = i + 1;
        break;
      }
    }
    // a.b.c is completely import name
    if (importEnd == chain.size()) {
      resultExpr = N<IdExpr>(importName);
      return;
    }
    auto fctx = importName.empty() ? ctx : ctx->cache->imports[importName].ctx;
    for (int i = int(chain.size()) - 1; i >= importEnd; i--) {
      auto s = join(chain, ".", importEnd, i + 1);
      val = fctx->find(s);
      // Make sure that we access only global imported variables.
      if (val && (importName.empty() || val->isGlobal())) {
        itemName = val->canonicalName;
        itemEnd = i + 1;
        if (!importName.empty())
          ctx->add(val->canonicalName, val);
        break;
      }
    }
    if (itemName.empty() && importName.empty())
      error("identifier '{}' not found", chain[importEnd]);
    if (itemName.empty())
      error("identifier '{}' not found in {}", chain[importEnd], importName);
    resultExpr = N<IdExpr>(itemName);
    if (importName.empty())
      resultExpr = transform(resultExpr.get(), true);
    if (val->isType() && itemEnd == chain.size())
      resultExpr->markType();
    for (int i = itemEnd; i < chain.size(); i++)
      resultExpr = N<DotExpr>(move(resultExpr), chain[i]);
  } else {
    resultExpr = N<DotExpr>(transform(expr->expr.get(), true), expr->member);
  }
}

void SimplifyVisitor::visit(SliceExpr *expr) {
  resultExpr = N<SliceExpr>(transform(expr->start), transform(expr->stop),
                            transform(expr->step));
}

void SimplifyVisitor::visit(EllipsisExpr *expr) {
  error("unexpected ellipsis expression");
}

void SimplifyVisitor::visit(TypeOfExpr *expr) {
  resultExpr = N<TypeOfExpr>(transform(expr->expr.get(), true));
  resultExpr->markType();
}

void SimplifyVisitor::visit(YieldExpr *expr) {
  if (!ctx->inFunction())
    error("expected function body");
  defaultVisit(expr);
}

void SimplifyVisitor::visit(LambdaExpr *expr) {
  vector<StmtPtr> stmts;
  stmts.push_back(N<ReturnStmt>(clone(expr->expr)));
  resultExpr = makeAnonFn(move(stmts), expr->vars);
}

void SimplifyVisitor::visit(AssignExpr *expr) {
  seqassert(expr->var->getId(), "only simple assignment expression are supported");
  if (!ctx->canAssign)
    error("assignment expression in a short-circuiting subexpression");
  vector<StmtPtr> s;
  s.push_back(N<AssignStmt>(clone(expr->var), clone(expr->expr)));
  resultExpr = transform(N<StmtExpr>(move(s), clone(expr->var)));
}

void SimplifyVisitor::visit(RangeExpr *expr) {
  error("unexpected pattern range expression");
}

void SimplifyVisitor::visit(StmtExpr *expr) {
  vector<StmtPtr> stmts;
  for (auto &s : expr->stmts)
    stmts.emplace_back(transform(s));
  resultExpr = N<StmtExpr>(move(stmts), transform(expr->expr));
}

/**************************************************************************************/

ExprPtr SimplifyVisitor::transformInt(const string &value, const string &suffix) {
  auto to_int = [](const string &s) {
    if (startswith(s, "0b") || startswith(s, "0B"))
      return std::stoull(s.substr(2), nullptr, 2);
    return std::stoull(s, nullptr, 0);
  };
  try {
    if (suffix.empty()) {
      auto expr = N<IntExpr>(to_int(value));
      expr->isStaticExpr = true;
      return expr;
    }
    /// Unsigned numbers: use UInt[64] for that
    if (suffix == "u")
      return transform(N<CallExpr>(N<IndexExpr>(N<IdExpr>("UInt"), N<IntExpr>(64)),
                                   N<IntExpr>(to_int(value))));
    /// Fixed-precision numbers (uXXX and iXXX)
    /// NOTE: you cannot use binary (0bXXX) format with those numbers.
    /// TODO: implement non-string constructor for these cases.
    if (suffix[0] == 'u' && isdigit(suffix.substr(1)))
      return transform(N<CallExpr>(
          N<IndexExpr>(N<IdExpr>("UInt"), N<IntExpr>(std::stoi(suffix.substr(1)))),
          N<StringExpr>(value)));
    if (suffix[0] == 'i' && isdigit(suffix.substr(1)))
      return transform(N<CallExpr>(
          N<IndexExpr>(N<IdExpr>("Int"), N<IntExpr>(std::stoi(suffix.substr(1)))),
          N<StringExpr>(value)));
  } catch (std::out_of_range &) {
    error("integer {} out of range", value);
  }
  /// Custom suffix sfx: use int.__suffix_sfx__(str) call.
  /// NOTE: you cannot neither use binary (0bXXX) format here.
  return transform(
      N<CallExpr>(N<DotExpr>(N<IdExpr>("int"), format("__suffix_{}__", suffix)),
                  N<StringExpr>(value)));
}

ExprPtr SimplifyVisitor::transformFString(string value) {
  vector<ExprPtr> items;
  int braceCount = 0, braceStart = 0;
  for (int i = 0; i < value.size(); i++) {
    if (value[i] == '{') {
      if (braceStart < i)
        items.push_back(N<StringExpr>(value.substr(braceStart, i - braceStart)));
      if (!braceCount)
        braceStart = i + 1;
      braceCount++;
    } else if (value[i] == '}') {
      braceCount--;
      if (!braceCount) {
        string code = value.substr(braceStart, i - braceStart);
        auto offset = getSrcInfo();
        offset.col += i;
        if (!code.empty() && code.back() == '=') {
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
  if (braceStart != value.size())
    items.push_back(N<StringExpr>(value.substr(braceStart, value.size() - braceStart)));
  return transform(N<CallExpr>(N<DotExpr>(N<IdExpr>("str"), "cat"), move(items)));
}

StmtPtr SimplifyVisitor::transformGeneratorBody(const vector<GeneratorBody> &loops,
                                                SuiteStmt *&prev) {
  StmtPtr suite = N<SuiteStmt>(), newSuite = nullptr;
  prev = (SuiteStmt *)suite.get();
  for (auto &l : loops) {
    newSuite = N<SuiteStmt>();
    auto nextPrev = (SuiteStmt *)newSuite.get();

    prev->stmts.push_back(N<ForStmt>(l.vars->clone(), l.gen->clone(), move(newSuite)));
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

ExprPtr SimplifyVisitor::makeAnonFn(vector<StmtPtr> &&stmts,
                                    const vector<string> &argNames) {
  vector<Param> params;
  vector<CallExpr::Arg> args;

  string name = ctx->cache->getTemporaryVar("lambda");
  for (auto &s : argNames)
    params.emplace_back(Param{s, nullptr, nullptr});
  auto s = transform(N<FunctionStmt>(name, nullptr, vector<Param>{}, move(params),
                                     N<SuiteStmt>(move(stmts)), Attr({Attr::Capture})));
  if (s)
    return N<StmtExpr>(move(s), transform(N<IdExpr>(name)));
  return transform(N<IdExpr>(name));
  //  return N<CallExpr>(N<IdExpr>(name), move(args));
}

} // namespace ast
} // namespace seq
