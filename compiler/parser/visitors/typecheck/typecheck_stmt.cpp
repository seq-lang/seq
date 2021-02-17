/*
 * typecheck_stmt.cpp --- Type inference for AST statements.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

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

namespace seq {
namespace ast {

using namespace types;

StmtPtr TypecheckVisitor::transform(const StmtPtr &stmt_) {
  auto &stmt = const_cast<StmtPtr &>(stmt_);
  if (!stmt || stmt->done)
    return move(stmt);
  TypecheckVisitor v(ctx);
  v.setSrcInfo(stmt->getSrcInfo());
  auto oldAge = ctx->age;
  stmt->age = ctx->age = std::max(stmt->age, oldAge);
  stmt->accept(v);
  ctx->age = oldAge;
  if (v.resultStmt)
    stmt = move(v.resultStmt);
  if (!v.prependStmts->empty()) {
    if (stmt)
      v.prependStmts->push_back(move(stmt));
    bool done = true;
    for (auto &s : *(v.prependStmts))
      done &= s->done;
    stmt = N<SuiteStmt>(move(*v.prependStmts));
    stmt->done = done;
  }
  return move(stmt);
}

void TypecheckVisitor::defaultVisit(Stmt *s) {
  seqassert(false, "unexpected AST node {}", s->toString());
}

/**************************************************************************************/

void TypecheckVisitor::visit(SuiteStmt *stmt) {
  vector<StmtPtr> stmts;
  stmt->done = true;
  for (auto &s : stmt->stmts)
    if (auto t = transform(s)) {
      stmts.push_back(move(t));
      stmt->done &= stmts.back()->done;
    }
  stmt->stmts = move(stmts);
}

void TypecheckVisitor::visit(PassStmt *stmt) { stmt->done = true; }

void TypecheckVisitor::visit(BreakStmt *stmt) { stmt->done = true; }

void TypecheckVisitor::visit(ContinueStmt *stmt) { stmt->done = true; }

void TypecheckVisitor::visit(ExprStmt *stmt) {
  // Make sure to allow expressions with void type.
  stmt->expr = transform(stmt->expr, false, true);
  stmt->done = stmt->expr->done;
}

void TypecheckVisitor::visit(AssignStmt *stmt) {
  // Simplify stage ensures that lhs is always IdExpr.
  string lhs;
  if (auto e = stmt->lhs->getId())
    lhs = e->value;
  seqassert(!lhs.empty(), "invalid AssignStmt {}", stmt->lhs->toString());
  stmt->rhs = transform(stmt->rhs);
  stmt->type = transformType(stmt->type);
  TypecheckItem::Kind kind;
  if (!stmt->rhs) { // Case 1: forward declaration: x: type
    stmt->lhs->type |= stmt->type ? stmt->type->getType()
                                  : ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel);
    ctx->add(kind = TypecheckItem::Var, lhs, stmt->lhs->type);
    stmt->done = realize(stmt->lhs->type) != nullptr;
  } else { // Case 2: Normal assignment
    if (stmt->type && stmt->type->getType()->getClass()) {
      auto t = ctx->instantiate(getSrcInfo(), stmt->type->getType());
      LOG_TYPECHECK("[inst] {} -> {}", stmt->lhs->toString(), t->toString());
      stmt->lhs->type |= t;
      wrapOptionalIfNeeded(stmt->lhs->getType(), stmt->rhs);
      stmt->lhs->type |= stmt->rhs->type;
    }
    kind = stmt->rhs->isType() ? TypecheckItem::Type
                               : (stmt->rhs->getType()->getFunc() ? TypecheckItem::Func
                                                                  : TypecheckItem::Var);
    ctx->add(kind, lhs, stmt->rhs->getType());
    stmt->done = stmt->rhs->done;
  }
  // Save the variable to the local realization context
  ctx->bases.back().visitedAsts[lhs] = {kind, stmt->lhs->type};
}

void TypecheckVisitor::visit(UpdateStmt *stmt) {
  stmt->lhs = transform(stmt->lhs);

  // Case 1: Check for atomic and in-place updates (a += b).
  // In-place updates (a += b) are stored as Update(a, Binary(a + b, inPlace=true)).
  auto b = const_cast<BinaryExpr *>(stmt->rhs->getBinary());
  if (b && b->inPlace) {
    bool noReturn = false;
    auto oldRhsType = stmt->rhs->type;
    if (auto nb = transformBinary(b, stmt->isAtomic, &noReturn))
      stmt->rhs = move(nb);
    oldRhsType |= stmt->rhs->type;
    if (stmt->rhs->getBinary()) { // still BinaryExpr: will be transformed later.
      stmt->lhs->type |= stmt->rhs->type;
      return;
    } else if (noReturn) { // remove assignment, call update function instead
                           // (__i***__ or __atomic_***__)
      bool done = stmt->rhs->done;
      resultStmt = N<ExprStmt>(move(stmt->rhs));
      resultStmt->done = done;
      return;
    }
  }
  // Case 2: Check for atomic min and max operations: a = min(a, ...).
  // NOTE: does not check for a = min(..., a).
  auto lhsClass = stmt->lhs->getType()->getClass();
  CallExpr *c;
  if (stmt->isAtomic && stmt->lhs->getId() &&
      (c = const_cast<CallExpr *>(stmt->rhs->getCall())) &&
      (c->expr->isId("min") || c->expr->isId("max")) && c->args.size() == 2 &&
      c->args[0].value->isId(string(stmt->lhs->getId()->value))) {
    auto ptrTyp =
        ctx->instantiateGeneric(getSrcInfo(), ctx->findInternal("Ptr"), {lhsClass});
    LOG_TYPECHECK("[inst] {} -> {}", stmt->lhs->toString(), ptrTyp->toString());
    c->args[1].value = transform(c->args[1].value);
    auto rhsTyp = c->args[1].value->getType()->getClass();
    if (auto method = ctx->findBestMethod(
            lhsClass.get(), format("__atomic_{}__", c->expr->getId()->value),
            {{"", ptrTyp}, {"", rhsTyp}})) {
      resultStmt = transform(N<ExprStmt>(N<CallExpr>(N<IdExpr>(method->funcName),
                                                     N<PtrExpr>(move(stmt->lhs)),
                                                     move(c->args[1].value))));
      return;
    }
  }

  stmt->rhs = transform(stmt->rhs);
  auto rhsClass = stmt->rhs->getType()->getClass();
  // Case 3: check for an atomic assignment.
  if (stmt->isAtomic && lhsClass && rhsClass) {
    auto ptrType =
        ctx->instantiateGeneric(getSrcInfo(), ctx->findInternal("Ptr"), {lhsClass});
    LOG_TYPECHECK("[inst] {} -> {}", stmt->lhs->toString(), ptrType->toString());
    if (auto m = ctx->findBestMethod(lhsClass.get(), "__atomic_xchg__",
                                     {{"", ptrType}, {"", rhsClass}})) {
      resultStmt = transform(N<ExprStmt>(N<CallExpr>(
          N<IdExpr>(m->funcName), N<PtrExpr>(move(stmt->lhs)), move(stmt->rhs))));
      return;
    }
    stmt->isAtomic = false;
  }
  // Case 4: handle optionals if needed.
  wrapOptionalIfNeeded(stmt->lhs->getType(), stmt->rhs);
  stmt->lhs->type |= stmt->rhs->type;
  stmt->done = stmt->rhs->done;
}

void TypecheckVisitor::visit(AssignMemberStmt *stmt) {
  stmt->lhs = transform(stmt->lhs);
  stmt->rhs = transform(stmt->rhs);
  auto lhsClass = stmt->lhs->getType()->getClass();

  if (lhsClass) {
    auto member = ctx->findMember(lhsClass->name, stmt->member);
    if (!member && lhsClass->name == "Optional") {
      // Unwrap optional and look up there:
      resultStmt = transform(
          N<AssignMemberStmt>(N<CallExpr>(N<IdExpr>("unwrap"), move(stmt->lhs)),
                              stmt->member, move(stmt->rhs)));
      return;
    }
    if (!member)
      error("cannot find '{}' in {}", stmt->member, lhsClass->name);
    if (lhsClass->getRecord())
      error("tuple element '{}' is read-only", stmt->member);
    auto typ = ctx->instantiate(getSrcInfo(), member, lhsClass.get());
    LOG_TYPECHECK("[inst] {} -> {}", stmt->lhs->toString(), typ->toString());
    wrapOptionalIfNeeded(typ, stmt->rhs);
    stmt->rhs->type |= typ;
    stmt->done = stmt->rhs->done;
  }
}

void TypecheckVisitor::visit(ReturnStmt *stmt) {
  stmt->expr = transform(stmt->expr);
  if (stmt->expr) {
    auto &base = ctx->bases.back();
    wrapOptionalIfNeeded(base.returnType, stmt->expr);
    base.returnType |= stmt->expr->type;
    auto retTyp = stmt->expr->getType()->getClass();
    stmt->done = stmt->expr->done;
  } else {
    stmt->done = true;
  }
}

void TypecheckVisitor::visit(YieldStmt *stmt) {
  if (stmt->expr)
    stmt->expr = transform(stmt->expr);
  auto baseTyp = stmt->expr ? stmt->expr->getType() : ctx->findInternal("void");
  auto t = ctx->instantiateGeneric(stmt->getSrcInfo(), ctx->findInternal("Generator"),
                                   {baseTyp});
  LOG_TYPECHECK("[inst] {} -> {}", stmt->toString(), t->toString());
  ctx->bases.back().returnType |= t;
  stmt->done = stmt->expr ? stmt->expr->done : true;
}

void TypecheckVisitor::visit(WhileStmt *stmt) {
  stmt->cond = transform(stmt->cond);
  stmt->suite = transform(stmt->suite);
  stmt->done = stmt->cond->done && stmt->suite->done;
}

void TypecheckVisitor::visit(ForStmt *stmt) {
  stmt->iter = transform(stmt->iter);
  // Extract the type of the for variable.
  if (!stmt->iter->getType()->canRealize())
    return; // continue after the iterator is realizable

  auto iterType = stmt->iter->getType()->getClass();
  if (auto tuple = iterType->getHeterogenousTuple()) {
    // Case 1: iterating heterogenous tuple.
    // Unroll a separate suite for each tuple member.
    // LOG("hetero");
    auto block = N<SuiteStmt>();
    auto tupleVar = ctx->cache->getTemporaryVar("tuple");
    block->stmts.push_back(N<AssignStmt>(N<IdExpr>(tupleVar), move(stmt->iter)));
    for (int ai = 0; ai < tuple->args.size(); ai++) {
      vector<StmtPtr> stmts;
      stmts.push_back(N<AssignStmt>(clone(stmt->var),
                                    N<IndexExpr>(N<IdExpr>(tupleVar), N<IntExpr>(ai))));
      stmts.push_back(clone(stmt->suite));
      block->stmts.push_back(N<SuiteStmt>(move(stmts), true));
    }
    resultStmt = transform(move(block));
  } else {
    // Case 2: iterating a generator. Standard for loop logic.
    if (iterType->name != "Generator" && !stmt->wrapped) {
      stmt->iter = transform(N<CallExpr>(N<DotExpr>(move(stmt->iter), "__iter__")));
      stmt->wrapped = true;
    }
    TypePtr varType = ctx->addUnbound(stmt->var->getSrcInfo(), ctx->typecheckLevel);
    if ((iterType = stmt->iter->getType()->getClass())) {
      if (iterType->name != "Generator")
        error("for loop expected a generator");
      varType |= iterType->generics[0].type;
      if (varType->is("void"))
        error("expression with void type");
    }
    string varName;
    if (auto e = stmt->var->getId())
      varName = e->value;
    seqassert(!varName.empty(), "empty for variable {}", stmt->var->toString());
    stmt->var->type |= varType;
    ctx->add(TypecheckItem::Var, varName, varType);
    stmt->suite = transform(stmt->suite);
    stmt->done = stmt->iter->done && stmt->suite->done;
  }
}

void TypecheckVisitor::visit(IfStmt *stmt) {
  seqassert(stmt->ifs.size() == 1 || (stmt->ifs.size() == 2 && !stmt->ifs[1].cond),
            "if not simplified");

  stmt->done = true;
  vector<IfStmt::If> ifs;
  bool includeElse = true, transformElse = true;
  auto cond = transform(stmt->ifs[0].cond);
  if (cond->isStaticExpr) {
    if (cond->staticEvaluation.first && !cond->staticEvaluation.second) {
      ; // do not include this suite
    } else if (!cond->staticEvaluation.first) {
      stmt->done = false; // do not typecheck this suite yet
      transformElse = false;
      ifs.emplace_back(IfStmt::If{move(cond), move(stmt->ifs[0].suite)});
    } else {
      if ((stmt->ifs[0].suite = transform(stmt->ifs[0].suite))) {
        ifs.emplace_back(IfStmt::If{move(cond), move(stmt->ifs[0].suite)});
      }
      includeElse = false;
    }
  } else {
    if (cond->type->getClass() && !cond->type->is("bool"))
      cond = transform(N<CallExpr>(N<DotExpr>(move(cond), "__bool__")));
    ifs.emplace_back(IfStmt::If{move(cond), transform(stmt->ifs[0].suite)});
  }
  if (!ifs.empty())
    stmt->done &= ifs.back().cond->done && ifs.back().suite->done;
  if (stmt->ifs.size() == 2 && includeElse) {
    ifs.emplace_back(IfStmt::If{nullptr, transformElse ? transform(stmt->ifs[1].suite)
                                                       : move(stmt->ifs[1].suite)});
    stmt->done &= ifs.back().suite->done;
  }
  if (!ifs.empty() && !ifs[0].cond)
    resultStmt = move(ifs[0].suite);
  else if (!ifs.empty())
    stmt->ifs = move(ifs);
  else
    resultStmt = transform(N<PassStmt>());
}

void TypecheckVisitor::visit(TryStmt *stmt) {
  vector<TryStmt::Catch> catches;
  stmt->suite = transform(stmt->suite);
  stmt->done &= stmt->suite->done;
  for (auto &c : stmt->catches) {
    c.exc = transformType(c.exc);
    if (!c.var.empty())
      ctx->add(TypecheckItem::Var, c.var, c.exc->getType());
    c.suite = transform(c.suite);
    stmt->done &= (c.exc ? c.exc->done : true) && c.suite->done;
  }
  stmt->finally = transform(stmt->finally);
  stmt->done &= stmt->finally->done;
}

void TypecheckVisitor::visit(ThrowStmt *stmt) {
  stmt->expr = transform(stmt->expr);
  auto tc = stmt->expr->type->getClass();
  if (!stmt->transformed && tc) {
    auto &f = ctx->cache->classes[tc->name].fields;
    if (f.empty() || !f[0].type->getClass() ||
        f[0].type->getClass()->name != "ExcHeader")
      error("cannot throw non-exception (first object member must be of type "
            "ExcHeader)");
    auto var = ctx->cache->getTemporaryVar("exc");
    vector<CallExpr::Arg> args;
    args.emplace_back(CallExpr::Arg{"", N<StringExpr>(tc->name)});
    args.emplace_back(CallExpr::Arg{
        "", N<DotExpr>(N<DotExpr>(N<IdExpr>(var),
                                  ctx->cache->classes[tc->name].fields[0].name),
                       "msg")});
    args.emplace_back(CallExpr::Arg{"", N<StringExpr>(ctx->bases.back().name)});
    args.emplace_back(CallExpr::Arg{"", N<StringExpr>(stmt->getSrcInfo().file)});
    args.emplace_back(CallExpr::Arg{"", N<IntExpr>(stmt->getSrcInfo().line)});
    args.emplace_back(CallExpr::Arg{"", N<IntExpr>(stmt->getSrcInfo().col)});
    resultStmt = transform(N<SuiteStmt>(
        N<AssignStmt>(N<IdExpr>(var), move(stmt->expr)),
        N<AssignMemberStmt>(N<IdExpr>(var),
                            ctx->cache->classes[tc->name].fields[0].name,
                            N<CallExpr>(N<IdExpr>("ExcHeader"), move(args))),
        N<ThrowStmt>(N<IdExpr>(var), true)));
  } else {
    stmt->done = false;
  }
}

void TypecheckVisitor::visit(FunctionStmt *stmt) {
  if (auto t = ctx->findInVisited(stmt->name).second) {
    // We realize built-ins and extern C function when we see them for the second time
    // (to avoid preamble realization).
    if (in(stmt->attributes, ATTR_FORCE_REALIZE) ||
        in(stmt->attributes, ATTR_EXTERN_C)) {
      if (!t->canRealize())
        error("builtins and external functions must be realizable");
      auto typ = ctx->instantiate(getSrcInfo(), t);
      LOG_TYPECHECK("[inst] fn {} -> {}", stmt->name, typ->toString());
      typ |= realize(typ->getFunc());
    }
    stmt->done = true;
    return;
  }

  // Parse preamble.
  auto &attributes = const_cast<FunctionStmt *>(stmt)->attributes;
  bool isClassMember = in(stmt->attributes, ATTR_PARENT_CLASS);
  auto explicits = parseGenerics(stmt->generics, ctx->typecheckLevel); // level down
  vector<TypePtr> generics;
  if (isClassMember && in(attributes, ATTR_NOT_STATIC)) {
    // Fetch parent class generics.
    auto parentClassAST = ctx->cache->classes[attributes[ATTR_PARENT_CLASS]].ast.get();
    auto parentClass = ctx->find(attributes[ATTR_PARENT_CLASS])->type->getClass();
    seqassert(parentClass, "parent class not set");
    for (int i = 0; i < parentClassAST->generics.size(); i++) {
      auto gen = parentClass->generics[i].type->getLink();
      generics.push_back(
          make_shared<LinkType>(LinkType::Unbound, parentClass->generics[i].id,
                                ctx->typecheckLevel - 1, nullptr, gen->isStatic));
      ctx->add(TypecheckItem::Type, parentClassAST->generics[i].name, generics.back(),
               gen->isStatic);
    }
  }
  for (const auto &i : stmt->generics)
    generics.push_back(ctx->find(i.name)->type);
  // Add function arguments.
  auto baseType =
      ctx->instantiate(getSrcInfo(),
                       ctx->find(generateCallableStub(stmt->args.size()))->type)
          ->getRecord();
  {
    ctx->typecheckLevel++;
    if (stmt->ret) {
      baseType->args[0] |= transformType(stmt->ret)->getType();
    } else {
      baseType->args[0] |= ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel);
      generics.push_back(baseType->args[0]);
    }
    for (int ai = 0; ai < stmt->args.size(); ai++) {
      if (!stmt->args[ai].type) {
        baseType->args[ai + 1] |= ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel);
        generics.push_back(baseType->args[ai + 1]);
      } else {
        baseType->args[ai + 1] |= transformType(stmt->args[ai].type)->getType();
      }
    }
    ctx->typecheckLevel--;
  }
  // Generalize generics.
  for (auto &g : generics) {
    seqassert(g && g->getLink() && g->getLink()->kind != types::LinkType::Link,
              "generic has been unified");
    if (g->getLink()->kind == LinkType::Unbound)
      g->getLink()->kind = LinkType::Generic;
  }
  // Construct the type.
  auto typ = make_shared<FuncType>(baseType, stmt->name, explicits);
  if (isClassMember && in(attributes, ATTR_NOT_STATIC))
    typ->funcParent = ctx->find(attributes[ATTR_PARENT_CLASS])->type;
  else if (in(attributes, ATTR_PARENT_FUNCTION))
    typ->funcParent = ctx->bases[ctx->findBase(attributes[ATTR_PARENT_FUNCTION])].type;
  typ->setSrcInfo(stmt->getSrcInfo());
  typ = std::static_pointer_cast<FuncType>(typ->generalize(ctx->typecheckLevel));
  // Check if this is a class method; if so, update the class method lookup table.
  if (isClassMember) {
    auto &methods = ctx->cache->classes[attributes[ATTR_PARENT_CLASS]]
                        .methods[ctx->cache->reverseIdentifierLookup[stmt->name]];
    bool found = false;
    for (auto &i : methods)
      if (i.name == stmt->name) {
        i.type = typ;
        found = true;
        break;
      }
    seqassert(found, "cannot find matching class method for {}", stmt->name);
  }
  // Update visited table.
  ctx->bases[ctx->findBase(attributes[ATTR_PARENT_FUNCTION])]
      .visitedAsts[stmt->name] = {TypecheckItem::Func, typ};
  ctx->add(TypecheckItem::Func, stmt->name, typ);
  LOG_REALIZE("[stmt] added func {}: {} (base={})", stmt->name, typ->toString(),
              ctx->getBase());
}

void TypecheckVisitor::visit(ClassStmt *stmt) {
  if (ctx->findInVisited(stmt->name).second && !in(stmt->attributes, "extend"))
    return;

  auto &attributes = const_cast<ClassStmt *>(stmt)->attributes;
  bool extension = in(attributes, "extend");
  ClassTypePtr typ = nullptr;
  if (!extension) {
    if (stmt->isRecord())
      typ = make_shared<RecordType>(stmt->name);
    else
      typ = make_shared<ClassType>(stmt->name);
    if (in(stmt->attributes, ATTR_TRAIT))
      typ->isTrait = true;
    typ->setSrcInfo(stmt->getSrcInfo());
    ctx->add(TypecheckItem::Type, stmt->name, typ);
    ctx->bases[ctx->findBase(attributes[ATTR_PARENT_FUNCTION])]
        .visitedAsts[stmt->name] = {TypecheckItem::Type, typ};

    // Parse class fields.
    typ->generics = parseGenerics(stmt->generics, ctx->typecheckLevel);
    {
      ctx->typecheckLevel++;
      for (auto ai = 0; ai < stmt->args.size(); ai++) {
        auto si = stmt->args[ai].type->getSrcInfo();
        ctx->cache->classes[stmt->name].fields[ai].type =
            transformType(stmt->args[ai].type)
                ->getType()
                ->generalize(ctx->typecheckLevel - 1);
        ctx->cache->classes[stmt->name].fields[ai].type->setSrcInfo(si);
        if (stmt->isRecord())
          typ->getRecord()->args.push_back(
              ctx->cache->classes[stmt->name].fields[ai].type);
      }
      ctx->typecheckLevel--;
    }
    // Remove lingering generics.
    for (const auto &g : stmt->generics) {
      auto val = ctx->find(g.name);
      seqassert(val && val->type && val->type->getLink() &&
                    val->type->getLink()->kind != types::LinkType::Link,
                "generic has been unified");
      if (val->type->getLink()->kind == LinkType::Unbound)
        val->type->getLink()->kind = LinkType::Generic;
      ctx->remove(g.name);
    }

    LOG_REALIZE("[class] {} -> {}", stmt->name, typ->toString());
    for (auto &m : ctx->cache->classes[stmt->name].fields)
      LOG_REALIZE("       - member: {}: {}", m.name, m.type->toString());
  }
  stmt->done = true;
}

/**************************************************************************************/

vector<types::Generic> TypecheckVisitor::parseGenerics(const vector<Param> &generics,
                                                       int level) {
  auto genericTypes = vector<Generic>();
  for (const auto &g : generics) {
    auto typ = ctx->addUnbound(getSrcInfo(), level, true, bool(g.type));
    genericTypes.emplace_back(Generic{g.name, typ->generalize(level),
                                      ctx->cache->unboundCount - 1, clone(g.deflt)});
    LOG_REALIZE("[generic] {} -> {} {}", g.name, typ->toString(), bool(g.type));
    ctx->add(TypecheckItem::Type, g.name, typ, bool(g.type));
  }
  return genericTypes;
}

} // namespace ast
} // namespace seq
