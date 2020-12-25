/*
 * typecheck_stmt.cpp --- Type inference for AST statements.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include "util/fmt/format.h"
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "parser/ast.h"
#include "parser/common.h"
#include "parser/visitors/simplify/simplify_ctx.h"
#include "parser/visitors/typecheck/typecheck.h"
#include "parser/visitors/typecheck/typecheck_ctx.h"

using fmt::format;
using std::deque;
using std::dynamic_pointer_cast;
using std::get;
using std::move;
using std::ostream;
using std::stack;
using std::static_pointer_cast;

string printParents(seq::ast::types::TypePtr t) {
  string s;
  for (auto p = t; p; p = t->getClass()->parent) {
    s = t->toString() + ":" + s;
  }
  return ":" + s;
}

namespace seq {
namespace ast {

using namespace types;

StmtPtr TypecheckVisitor::transform(const StmtPtr &stmt) {
  if (!stmt)
    return nullptr;
  TypecheckVisitor v(ctx);
  v.setSrcInfo(stmt->getSrcInfo());
  stmt->accept(v);
  if (v.prependStmts->size()) {
    if (v.resultStmt)
      v.prependStmts->push_back(move(v.resultStmt));
    v.resultStmt = N<SuiteStmt>(move(*v.prependStmts));
  }
  return move(v.resultStmt);
}

void TypecheckVisitor::defaultVisit(const Stmt *s) { resultStmt = s->clone(); }

void TypecheckVisitor::visit(const SuiteStmt *stmt) {
  vector<StmtPtr> r;
  if (stmt->ownBlock)
    ctx->addBlock();
  for (auto &s : stmt->stmts)
    if (auto t = transform(s))
      r.push_back(move(t));
  if (stmt->ownBlock)
    ctx->popBlock();
  resultStmt = N<SuiteStmt>(move(r), stmt->ownBlock);
}

void TypecheckVisitor::visit(const ExprStmt *stmt) {
  resultStmt = N<ExprStmt>(transform(stmt->expr));
}

void TypecheckVisitor::visit(const AssignStmt *stmt) {
  auto l = stmt->lhs->getId();
  // LOG("{}", stmt->toString());
  seqassert(l, "invalid AssignStmt {}", stmt->toString());

  auto rhs = transform(stmt->rhs);
  auto typExpr = transformType(stmt->type);
  types::TypePtr t;
  TypecheckItem::Kind k;
  if (!rhs) { // declarations
    t = typExpr ? typExpr->getType()
                : ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel);
    ctx->add(k = TypecheckItem::Var, l->value, t);
  } else {
    if (typExpr && typExpr->getType()->getClass()) {
      auto typ = ctx->instantiate(getSrcInfo(), typExpr->getType());

      auto lc = typ->getClass();
      auto rc = rhs->getType()->getClass();
      if (lc && lc->name == "Optional" && rc && rc->name != lc->name)
        rhs = transform(N<CallExpr>(N<IdExpr>("Optional"), move(rhs)));

      forceUnify(typ, rhs->getType());
    }
    k = rhs->isType()
            ? TypecheckItem::Type
            : (rhs->getType()->getFunc() ? TypecheckItem::Func : TypecheckItem::Var);
    ctx->add(k, l->value, t = rhs->getType());
  }
  //  if (l->value[0] == '.')
  ctx->bases.back().visitedAsts[l->value] = {k, t};
  auto lhs = clone(stmt->lhs);
  lhs->setType(forceUnify(lhs, t));
  resultStmt = N<AssignStmt>(move(lhs), move(rhs), move(typExpr));
}

void TypecheckVisitor::visit(const UpdateStmt *stmt) {
  auto l = transform(stmt->lhs);
  auto lc = l->getType()->getClass();
  ExprPtr r = nullptr;

  auto b = CAST(stmt->rhs, BinaryExpr);
  if (b && b->inPlace) {
    bool noReturn = false;
    auto e =
        transformBinary(b->lexpr, b->rexpr, b->op, true, stmt->isAtomic, &noReturn);
    e->setType(forceUnify(stmt->rhs, e->getType()));
    if (CAST(e, BinaryExpr)) { // decide later...
      l->setType(forceUnify(e.get(), l->getType()));
      resultStmt = N<UpdateStmt>(move(l), move(e), stmt->isAtomic);
      return;
    } else if (noReturn) { // Remove assignment, just use update stuff
      resultStmt = N<ExprStmt>(move(e));
      return;
    } else {
      r = move(e);
    }
  }
  // detect min/max: a = min(a, ...) (not vice-versa...)

  bool atomic = stmt->isAtomic;
  const CallExpr *c;
  if (atomic && l->getId() && (c = stmt->rhs->getCall()) &&
      (c->expr->isId("min") || c->expr->isId("max")) && c->args.size() == 2 &&
      c->args[0].value->isId(string(l->getId()->value))) {
    auto pt = ctx->instantiateGeneric(getSrcInfo(), ctx->findInternal("Ptr"), {lc});
    auto rsh = transform(c->args[1].value);
    auto rc = rsh->getType()->getClass();
    if (auto m =
            findBestCall(lc, format("__atomic_{}__", chop(c->expr->getId()->value)),
                         {{"", pt}, {"", rc}})) {
      resultStmt = transform(
          N<ExprStmt>(N<CallExpr>(N<IdExpr>(m->name), N<PtrExpr>(move(l)), move(rsh))));
      return;
    }
  }

  if (!r)
    r = transform(stmt->rhs);
  auto rc = r->getType()->getClass();
  if (atomic && lc && rc) { // maybe an atomic = ?
    auto pt = ctx->instantiateGeneric(getSrcInfo(), ctx->findInternal("Ptr"), {lc});
    if (auto m = findBestCall(lc, "__atomic_xchg__", {{"", pt}, {"", rc}})) {
      resultStmt = transform(
          N<ExprStmt>(N<CallExpr>(N<IdExpr>(m->name), N<PtrExpr>(move(l)), move(r))));
      return;
    } else {
      atomic = false;
    }
  } else if (lc && lc->name == "Optional" && rc && rc->name != lc->name) {
    r = transform(N<CallExpr>(N<IdExpr>("Optional"), move(r)));
  }
  l->setType(forceUnify(r.get(), l->getType()));
  resultStmt = N<UpdateStmt>(move(l), move(r), atomic);
} // namespace ast

void TypecheckVisitor::visit(const AssignMemberStmt *stmt) {
  auto lh = transform(stmt->lhs);
  auto rh = transform(stmt->rhs);
  auto lc = lh->getType()->getClass();
  auto rc = rh->getType()->getClass();

  if (lc) {
    auto mm = ctx->findMember(lc->name, stmt->member);
    if (!mm && lc->name == "Optional") {
      resultStmt = transform(N<AssignMemberStmt>(
          N<CallExpr>(N<IdExpr>("unwrap"), clone(stmt->lhs)), stmt->member, move(rh)));
      return;
    }
    if (!mm)
      error("cannot find '{}'", stmt->member);

    if (lc && lc->isRecord())
      error("records are read-only ^ {} , {}", lc->toString(), lh->toString());

    auto t = ctx->instantiate(getSrcInfo(), mm, lc.get());
    lc = t->getClass();
    if (lc && lc->name == "Optional" && rc && rc->name != lc->name)
      rh = transform(N<CallExpr>(N<IdExpr>("Optional"), move(rh)));
    forceUnify(t, rh->getType());
  }

  resultStmt = N<AssignMemberStmt>(move(lh), stmt->member, move(rh));
}

void TypecheckVisitor::visit(const ReturnStmt *stmt) {
  auto n = ctx->bases.back().name;
  if (stmt->expr) {
    auto e = transform(stmt->expr);
    auto &base = ctx->bases.back();

    if (base.returnType) {
      auto l = base.returnType->getClass();
      auto r = e->getType()->getClass();
      if (l && r && r->name != l->name) {
        if (l->name == "Optional") {
          e = transform(N<CallExpr>(N<IdExpr>("Optional"), move(e)));
        }
        // For now this only works if we already know that returnType is optional
      }
      forceUnify(e->getType(), base.returnType);
    } else {
      base.returnType = e->getType();
    }

    // HACK for return void in Partial.__call__
    if (startswith(base.name, "Partial.N") && endswith(base.name, ".__call__")) {
      auto c = e->getType()->getClass();
      if (c && c->name == "void") {
        resultStmt = N<ExprStmt>(move(e));
        return;
      }
    }
    resultStmt = N<ReturnStmt>(move(e));
  } else {
    resultStmt = N<ReturnStmt>(nullptr);
  }
}

void TypecheckVisitor::visit(const YieldStmt *stmt) {
  types::TypePtr t = nullptr;
  if (stmt->expr) {
    auto e = transform(stmt->expr);
    t = ctx->instantiateGeneric(e->getSrcInfo(), ctx->findInternal("Generator"),
                                {e->getType()});
    resultStmt = N<YieldStmt>(move(e));
  } else {
    t = ctx->instantiateGeneric(stmt->getSrcInfo(), ctx->findInternal("Generator"),
                                {ctx->findInternal("void")});
    resultStmt = N<YieldStmt>(nullptr);
  }
  auto &base = ctx->bases.back();
  if (base.returnType)
    forceUnify(t, base.returnType);
  else
    base.returnType = t;
}

void TypecheckVisitor::visit(const DelStmt *stmt) {
  auto expr = CAST(stmt->expr, IdExpr);
  ctx->remove(expr->value);
}

void TypecheckVisitor::visit(const WhileStmt *stmt) {
  resultStmt = N<WhileStmt>(transform(stmt->cond), transform(stmt->suite));
}

void TypecheckVisitor::visit(const ForStmt *stmt) {
  auto iter = transform(stmt->iter);
  TypePtr varType = ctx->addUnbound(stmt->var->getSrcInfo(), ctx->typecheckLevel);
  if (!iter->getType()->getUnbound()) {
    auto iterType = iter->getType()->getClass();
    if (!iterType || iterType->name != "Generator")
      error(iter, "expected a generator");
    forceUnify(varType, iterType->explicits[0].type);
  }
  ctx->addBlock();
  auto i = CAST(stmt->var, IdExpr);
  assert(i);
  string varName = i->value;
  ctx->add(TypecheckItem::Var, varName, varType);
  resultStmt = N<ForStmt>(transform(stmt->var), move(iter), transform(stmt->suite));
  ctx->popBlock();
}

void TypecheckVisitor::visit(const IfStmt *stmt) {
  vector<IfStmt::If> ifs;
  for (auto &i : stmt->ifs)
    ifs.push_back({transform(i.cond), transform(i.suite)});
  resultStmt = N<IfStmt>(move(ifs));
}

void TypecheckVisitor::visit(const MatchStmt *stmt) {
  auto w = transform(stmt->what);
  auto matchType = w->getType();
  auto matchTypeClass = matchType->getClass();

  auto unifyType = [&](TypePtr t) {
    // auto tc = t->getClass();
    // if (tc && tc->name == ".seq" && matchTypeClass && matchTypeClass->name ==
    // ".Kmer")
    //   return;
    assert(t && matchType);
    types::Type::Unification us;
    us.isMatch = true;
    if (t->unify(matchType, us) < 0) {
      us.undo();
      error("cannot unify {} and {}", t->toString(), matchType->toString());
    }
  };

  vector<PatternPtr> patterns;
  vector<StmtPtr> cases;
  for (auto ci = 0; ci < stmt->cases.size(); ci++) {
    ctx->addBlock();
    if (auto p = CAST(stmt->patterns[ci], BoundPattern)) {
      auto boundPat = transform(p->pattern);
      ctx->add(TypecheckItem::Var, p->var, boundPat->getType());
      patterns.push_back(move(boundPat));
      unifyType(patterns.back()->getType());
      cases.push_back(transform(stmt->cases[ci]));
    } else {
      patterns.push_back(transform(stmt->patterns[ci]));
      unifyType(patterns.back()->getType());
      cases.push_back(transform(stmt->cases[ci]));
    }
    ctx->popBlock();
  }
  resultStmt = N<MatchStmt>(move(w), move(patterns), move(cases));
}

void TypecheckVisitor::visit(const TryStmt *stmt) {
  vector<TryStmt::Catch> catches;
  auto suite = transform(stmt->suite);
  for (auto &c : stmt->catches) {
    ctx->addBlock();
    auto exc = transformType(c.exc);
    if (c.var != "")
      ctx->add(TypecheckItem::Var, c.var, exc->getType());
    catches.push_back({c.var, move(exc), transform(c.suite)});
    ctx->popBlock();
  }
  resultStmt = N<TryStmt>(move(suite), move(catches), transform(stmt->finally));
}

void TypecheckVisitor::visit(const ThrowStmt *stmt) {
  resultStmt = N<ThrowStmt>(transform(stmt->expr));
}

void TypecheckVisitor::visit(const FunctionStmt *stmt) {
  resultStmt = N<FunctionStmt>(stmt->name, nullptr, vector<Param>(), vector<Param>(),
                               nullptr, map<string, string>(stmt->attributes));
  bool isClassMember = in(stmt->attributes, ATTR_PARENT_CLASS);

  if (auto t = ctx->findInVisited(stmt->name).second) {
    // seeing these for the second time, realize them (not in the preamble though)
    if (in(stmt->attributes, ATTR_BUILTIN) || in(stmt->attributes, ATTR_EXTERN_C)) {
      if (!t->canRealize())
        error("builtins and external functions must be realizable");
      realizeFunc(ctx->instantiate(getSrcInfo(), t)->getFunc());
    }
    return;
  }

  auto &attributes = const_cast<FunctionStmt *>(stmt)->attributes;

  ctx->addBlock();
  auto explicits = parseGenerics(stmt->generics, ctx->typecheckLevel); // level down
  vector<TypePtr> generics;
  // Iterate parent!!!
  if (isClassMember && in(attributes, ATTR_NOT_STATIC)) {
    auto c = ctx->cache->classes[attributes[ATTR_PARENT_CLASS]].ast.get();
    auto ct = ctx->find(attributes[ATTR_PARENT_CLASS])->type->getClass();
    assert(ct);
    for (int i = 0; i < c->generics.size(); i++) {
      auto l = ct->explicits[i].type->getLink();
      auto gt = make_shared<LinkType>(LinkType::Unbound, ct->explicits[i].id,
                                      ctx->typecheckLevel - 1, nullptr, l->isStatic);
      generics.push_back(gt);
      ctx->add(TypecheckItem::Type, c->generics[i].name, gt, l->isStatic);
    }
  }
  for (auto &i : stmt->generics)
    generics.push_back(ctx->find(i.name)->type);

  ctx->typecheckLevel++;
  vector<TypePtr> args;
  if (stmt->ret) {
    args.push_back(transformType(stmt->ret)->getType());
  } else {
    args.push_back(ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel));
    generics.push_back(args.back());
  }
  for (auto &a : stmt->args) {
    args.push_back(a.type ? transformType(a.type)->getType()
                          : ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel));
    if (!a.type)
      generics.push_back(args.back());
    ctx->add(TypecheckItem::Var, a.name, args.back());
  }
  ctx->typecheckLevel--;
  for (auto &g : generics) { // Generalize generics
    assert(g && g->getLink() && g->getLink()->kind != types::LinkType::Link);
    if (g->getLink()->kind == LinkType::Unbound)
      g->getLink()->kind = LinkType::Generic;
  }
  ctx->popBlock();

  auto t = make_shared<FuncType>(
      stmt->name,
      ctx->findInternal(format("Function.N{}", stmt->args.size()))->getClass().get(),
      args, explicits);

  if (isClassMember && in(attributes, ATTR_NOT_STATIC)) {
    auto val = ctx->find(attributes[ATTR_PARENT_CLASS]);
    assert(val && val->type);
    t->parent = val->type;
  } else if (in(attributes, ATTR_PARENT_FUNCTION)) {
    t->parent = ctx->bases[ctx->findBase(attributes[ATTR_PARENT_FUNCTION])].type;
  }
  if (isClassMember) {
    auto &v = ctx->cache->classes[attributes[ATTR_PARENT_CLASS]]
                  .methods[ctx->cache->reverseIdentifierLookup[stmt->name]];
    bool found = false;
    for (auto &i : v) {
      if (i.name == stmt->name) {
        i.type = t;
        found = true;
        break;
      }
    }
    seqassert(found, "cannot find matching class method for {}", stmt->name);
  }

  t->setSrcInfo(stmt->getSrcInfo());
  t = std::static_pointer_cast<FuncType>(t->generalize(ctx->typecheckLevel));
  LOG_REALIZE("[stmt] added func {}: {} (base={}; parent={})", stmt->name,
              t->toString(), ctx->getBase(), printParents(t->parent));

  ctx->bases[ctx->findBase(attributes[ATTR_PARENT_FUNCTION])]
      .visitedAsts[stmt->name] = {TypecheckItem::Func, t};
  ctx->add(TypecheckItem::Func, stmt->name, t);
}

void TypecheckVisitor::visit(const ClassStmt *stmt) {
  if (ctx->findInVisited(stmt->name).second && !in(stmt->attributes, "extend"))
    resultStmt = N<ClassStmt>(stmt->name, vector<Param>(), vector<Param>(),
                              N<SuiteStmt>(), map<string, string>(stmt->attributes));
  else
    resultStmt = N<SuiteStmt>(parseClass(stmt));

  if (in(stmt->attributes, "extend"))
    ctx->extendEtape++;
}

vector<StmtPtr> TypecheckVisitor::parseClass(const ClassStmt *stmt) {
  bool extension = in(stmt->attributes, "extend");

  vector<StmtPtr> stmts;
  stmts.push_back(N<ClassStmt>(stmt->name, vector<Param>(), vector<Param>(),
                               N<SuiteStmt>(), map<string, string>(stmt->attributes)));

  ClassTypePtr ct;
  if (!extension) {
    auto &attributes = const_cast<ClassStmt *>(stmt)->attributes;
    ct = make_shared<ClassType>(stmt->name, stmt->isRecord(), vector<TypePtr>(),
                                vector<Generic>(), nullptr);
    if (in(stmt->attributes, "trait"))
      ct->isTrait = true;
    ct->setSrcInfo(stmt->getSrcInfo());
    auto ctxi = make_shared<TypecheckItem>(TypecheckItem::Type, ct);
    ctx->add(stmt->name, ctxi);
    ctx->bases[ctx->findBase(attributes[ATTR_PARENT_FUNCTION])]
        .visitedAsts[stmt->name] = {TypecheckItem::Type, ct};

    ct->explicits = parseGenerics(stmt->generics, ctx->typecheckLevel);
    ctx->typecheckLevel++;
    for (auto ai = 0; ai < stmt->args.size(); ai++) {
      // TODO: assert if t is generalized!
      ctx->cache->classes[stmt->name].fields[ai].type =
          transformType(stmt->args[ai].type)
              ->getType()
              ->generalize(ctx->typecheckLevel - 1);
      if (stmt->isRecord())
        ct->args.push_back(ctx->cache->classes[stmt->name].fields[ai].type);
    }
    ctx->typecheckLevel--;

    for (auto &g : stmt->generics) {
      auto val = ctx->find(g.name);
      if (auto g = val->type) {
        assert(g && g->getLink() && g->getLink()->kind != types::LinkType::Link);
        if (g->getLink()->kind == LinkType::Unbound)
          g->getLink()->kind = LinkType::Generic;
      }
      ctx->remove(g.name);
    }

    LOG_REALIZE("[class] {} (parent={})", ct->toString(), printParents(ct->parent));
    for (auto &m : ctx->cache->classes[stmt->name].fields)
      LOG_REALIZE("       - member: {}: {}", m.name, m.type->toString());
  }

  return stmts;
}

vector<types::Generic> TypecheckVisitor::parseGenerics(const vector<Param> &generics,
                                                       int level) {
  auto genericTypes = vector<types::Generic>();
  for (auto &g : generics) {
    assert(!g.name.empty());
    auto tp = ctx->addUnbound(getSrcInfo(), level, true, bool(g.type));
    genericTypes.push_back(
        {g.name, tp->generalize(level), ctx->cache->unboundCount - 1, clone(g.deflt)});
    LOG_REALIZE("[generic] {} -> {} {}", g.name, tp->toString(), bool(g.type));
    ctx->add(TypecheckItem::Type, g.name, tp, bool(g.type));
    /*auto tg = ctx->find(g.name)->type;
    assert(tg->getLink() && tg->getLink()->kind == LinkType::Generic);
    genericTypes.emplace_back(
        types::Generic{g.name, tg, tg->getLink()->id, clone(g.deflt)});
    //    LOG_REALIZE("[generic] {} -> {} {}", g.name, tg->toString(), bool(g.type));
    ctx->add(TypecheckItem::Type, g.name,
             make_shared<LinkType>(LinkType::Unbound, tg->getLink()->id, level,
    nullptr, tg->getLink()->isStatic), false, true, tg->getLink()->isStatic);*/
  }
  return genericTypes;
}

void TypecheckVisitor::addFunctionGenerics(FuncTypePtr t) {
  int pi = 0;
  for (auto p = t->parent; p; pi++) {
    if (auto y = p->getFunc()) {
      for (auto &g : y->explicits)
        if (auto s = g.type->getStatic())
          ctx->add(TypecheckItem::Type, g.name, s, true);
        else if (!g.name.empty())
          ctx->add(TypecheckItem::Type, g.name, g.type);
      p = y->parent;
    } else {
      auto c = p->getClass();
      assert(c);
      for (auto &g : c->explicits)
        if (auto s = g.type->getStatic())
          ctx->add(TypecheckItem::Type, g.name, s, true);
        else if (!g.name.empty())
          ctx->add(TypecheckItem::Type, g.name, g.type);
      p = c->parent;
    }
  }
  for (auto &g : t->explicits)
    if (auto s = g.type->getStatic())
      ctx->add(TypecheckItem::Type, g.name, s, true);
    else if (!g.name.empty())
      ctx->add(TypecheckItem::Type, g.name, g.type);
}

} // namespace ast
} // namespace seq