/*
 * simplify_statement.cpp --- AST statement simplifications.
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
#include "parser/ocaml/ocaml.h"
#include "parser/visitors/format/format.h"
#include "parser/visitors/simplify/simplify.h"

using fmt::format;

namespace seq {
namespace ast {

StmtPtr SimplifyVisitor::transform(const Stmt *stmt) {
  if (!stmt)
    return nullptr;

  SimplifyVisitor v(ctx, preamble);
  v.setSrcInfo(stmt->getSrcInfo());
  const_cast<Stmt *>(stmt)->accept(v);
  if (v.resultStmt)
    v.resultStmt->age = ctx->cache->age;
  return move(v.resultStmt);
}

StmtPtr SimplifyVisitor::transform(const StmtPtr &stmt) {
  return transform(stmt.get());
}

void SimplifyVisitor::defaultVisit(Stmt *s) { resultStmt = s->clone(); }

/**************************************************************************************/

void SimplifyVisitor::visit(SuiteStmt *stmt) {
  vector<StmtPtr> r;
  // Make sure to add context blocks if this suite requires it...
  if (stmt->ownBlock)
    ctx->addBlock();
  for (const auto &s : stmt->stmts)
    SuiteStmt::flatten(transform(s), r);
  // ... and to remove it later.
  if (stmt->ownBlock)
    ctx->popBlock();
  resultStmt = N<SuiteStmt>(move(r), stmt->ownBlock);
}

void SimplifyVisitor::visit(ContinueStmt *stmt) {
  if (ctx->loops.empty())
    error("continue outside of a loop");
  resultStmt = stmt->clone();
}

void SimplifyVisitor::visit(BreakStmt *stmt) {
  if (ctx->loops.empty())
    error("break outside of a loop");
  if (!ctx->loops.back().empty()) {
    resultStmt = N<SuiteStmt>(
        transform(N<AssignStmt>(N<IdExpr>(ctx->loops.back()), N<BoolExpr>(false))),
        stmt->clone());
  } else {
    resultStmt = stmt->clone();
  }
}

void SimplifyVisitor::visit(ExprStmt *stmt) {
  resultStmt = N<ExprStmt>(transform(stmt->expr));
}

void SimplifyVisitor::visit(AssignStmt *stmt) {
  vector<StmtPtr> stmts;
  if (stmt->rhs && stmt->rhs->getBinary() && stmt->rhs->getBinary()->inPlace) {
    /// Case 1: a += b
    seqassert(!stmt->type, "invalid AssignStmt {}", stmt->toString());
    stmts.push_back(
        transformAssignment(stmt->lhs.get(), stmt->rhs.get(), nullptr, false, true));
  } else if (stmt->type) {
    /// Case 2:
    stmts.push_back(transformAssignment(stmt->lhs.get(), stmt->rhs.get(),
                                        stmt->type.get(), true, false));
  } else {
    unpackAssignments(stmt->lhs.get(), stmt->rhs.get(), stmts, stmt->shadow, false);
  }
  resultStmt = stmts.size() == 1 ? move(stmts[0]) : N<SuiteStmt>(move(stmts));
}

void SimplifyVisitor::visit(DelStmt *stmt) {
  if (auto eix = stmt->expr->getIndex()) {
    resultStmt = N<ExprStmt>(transform(
        N<CallExpr>(N<DotExpr>(clone(eix->expr), "__delitem__"), clone(eix->index))));
  } else if (auto ei = stmt->expr->getId()) {
    resultStmt = transform(N<AssignStmt>(
        clone(stmt->expr), N<CallExpr>(N<TypeOfExpr>(clone(stmt->expr)))));
    ctx->remove(ei->value);
  } else {
    error("invalid del statement");
  }
}

void SimplifyVisitor::visit(PrintStmt *stmt) {
  vector<CallExpr::Arg> args;
  for (auto &i : stmt->items)
    args.emplace_back(CallExpr::Arg{"", transform(i)});
  if (stmt->isInline)
    args.emplace_back(CallExpr::Arg{"end", N<StringExpr>(" ")});
  resultStmt = N<ExprStmt>(N<CallExpr>(transform(N<IdExpr>("print")), move(args)));
}

void SimplifyVisitor::visit(ReturnStmt *stmt) {
  if (!ctx->inFunction())
    error("expected function body");
  resultStmt = N<ReturnStmt>(transform(stmt->expr));
}

void SimplifyVisitor::visit(YieldStmt *stmt) {
  if (!ctx->inFunction())
    error("expected function body");
  resultStmt = N<YieldStmt>(transform(stmt->expr));
}

void SimplifyVisitor::visit(YieldFromStmt *stmt) {
  auto var = ctx->cache->getTemporaryVar("yield");
  resultStmt = transform(
      N<ForStmt>(N<IdExpr>(var), clone(stmt->expr), N<YieldStmt>(N<IdExpr>(var))));
}

void SimplifyVisitor::visit(AssertStmt *stmt) {
  seqassert(!stmt->message || stmt->message->getString(),
            "assert message not a string");

  if (ctx->getLevel() && ctx->bases.back().attributes & FLAG_TEST)
    resultStmt = transform(N<IfStmt>(
        N<UnaryExpr>("!", clone(stmt->expr)),
        N<ExprStmt>(N<CallExpr>(
            N<DotExpr>(N<IdExpr>("__internal__"), "seq_assert_test"),
            N<StringExpr>(stmt->getSrcInfo().file), N<IntExpr>(stmt->getSrcInfo().line),
            stmt->message ? clone(stmt->message) : N<StringExpr>("")))));
  else
    resultStmt = transform(N<IfStmt>(
        N<UnaryExpr>("!", clone(stmt->expr)),
        N<ThrowStmt>(N<CallExpr>(
            N<DotExpr>(N<IdExpr>("__internal__"), "seq_assert"),
            N<StringExpr>(stmt->getSrcInfo().file), N<IntExpr>(stmt->getSrcInfo().line),
            stmt->message ? clone(stmt->message) : N<StringExpr>("")))));
}

void SimplifyVisitor::visit(WhileStmt *stmt) {
  ExprPtr cond = N<CallExpr>(N<DotExpr>(clone(stmt->cond), "__bool__"));
  string breakVar;
  StmtPtr assign = nullptr;
  if (stmt->elseSuite && stmt->elseSuite->firstInBlock()) {
    breakVar = ctx->cache->getTemporaryVar("no_break");
    assign =
        transform(N<AssignStmt>(N<IdExpr>(breakVar), N<BoolExpr>(true), nullptr, true));
  }
  ctx->loops.push_back(breakVar); // needed for transforming break in loop..else blocks
  StmtPtr whileStmt = N<WhileStmt>(transform(cond), transform(stmt->suite));
  ctx->loops.pop_back();
  if (stmt->elseSuite && stmt->elseSuite->firstInBlock()) {
    resultStmt = N<SuiteStmt>(
        move(assign), move(whileStmt),
        N<IfStmt>(transform(N<CallExpr>(N<DotExpr>(N<IdExpr>(breakVar), "__bool__"))),
                  transform(stmt->elseSuite)));
  } else {
    resultStmt = move(whileStmt);
  }
}

void SimplifyVisitor::visit(ForStmt *stmt) {
  string breakVar;
  auto iter = transform(stmt->iter); // needs in-advance transformation to prevent
                                     // name clashes with the iterator variable
  StmtPtr assign = nullptr, forStmt = nullptr;
  if (stmt->elseSuite && stmt->elseSuite->firstInBlock()) {
    breakVar = ctx->cache->getTemporaryVar("no_break");
    assign =
        transform(N<AssignStmt>(N<IdExpr>(breakVar), N<BoolExpr>(true), nullptr, true));
  }
  ctx->loops.push_back(breakVar); // needed for transforming break in loop..else blocks
  ctx->addBlock();
  if (auto i = stmt->var->getId()) {
    ctx->add(SimplifyItem::Var, i->value, ctx->generateCanonicalName(i->value));
    forStmt = N<ForStmt>(transform(stmt->var), clone(iter), transform(stmt->suite));
  } else {
    string varName = ctx->cache->getTemporaryVar("for");
    ctx->add(SimplifyItem::Var, varName, varName);
    auto var = N<IdExpr>(varName);
    vector<StmtPtr> stmts;
    stmts.push_back(N<AssignStmt>(clone(stmt->var), clone(var), nullptr, true));
    stmts.push_back(clone(stmt->suite));
    forStmt = N<ForStmt>(clone(var), clone(iter), transform(N<SuiteStmt>(move(stmts))));
  }
  ctx->popBlock();
  ctx->loops.pop_back();

  if (stmt->elseSuite && stmt->elseSuite->firstInBlock()) {
    resultStmt = N<SuiteStmt>(
        move(assign), move(forStmt),
        N<IfStmt>(transform(N<CallExpr>(N<DotExpr>(N<IdExpr>(breakVar), "__bool__"))),
                  transform(stmt->elseSuite)));
  } else {
    resultStmt = move(forStmt);
  }
}

void SimplifyVisitor::visit(IfStmt *stmt) {
  seqassert(!stmt->ifs.empty() && stmt->ifs[0].cond, "invalid if statement");

  vector<IfStmt::If> topIf;
  topIf.emplace_back(
      IfStmt::If{transform(clone(stmt->ifs[0].cond)), transform(stmt->ifs[0].suite)});
  vector<IfStmt::If> subIf;
  for (auto i = 1; i < stmt->ifs.size(); i++) {
    seqassert(stmt->ifs[i].cond || i == int(stmt->ifs.size() - 1),
              "else that is not last condition");
    subIf.push_back({clone(stmt->ifs[i].cond), clone(stmt->ifs[i].suite)});
  }
  if (!subIf.empty()) {
    if (!subIf[0].cond)
      topIf.emplace_back(IfStmt::If{nullptr, transform(subIf[0].suite)});
    else
      topIf.emplace_back(IfStmt::If{nullptr, transform(N<IfStmt>(move(subIf)))});
  }
  resultStmt = N<IfStmt>(move(topIf));
}

void SimplifyVisitor::visit(MatchStmt *stmt) {
  auto var = ctx->cache->getTemporaryVar("match");
  auto result = N<SuiteStmt>();
  result->stmts.push_back(
      N<AssignStmt>(N<IdExpr>(var), clone(stmt->what), nullptr, true));
  for (auto &c : stmt->cases) {
    ctx->addBlock();
    StmtPtr suite = N<SuiteStmt>(clone(c.suite), N<BreakStmt>());
    if (c.guard)
      suite = N<IfStmt>(clone(c.guard), move(suite));
    result->stmts.push_back(
        transformPattern(N<IdExpr>(var), clone(c.pattern), move(suite)));
    ctx->popBlock();
  }
  result->stmts.push_back(N<BreakStmt>()); // break even if there is no case _.
  resultStmt = transform(N<WhileStmt>(N<BoolExpr>(true), move(result)));
}

void SimplifyVisitor::visit(TryStmt *stmt) {
  vector<TryStmt::Catch> catches;
  auto suite = transform(stmt->suite);
  for (auto &ctch : stmt->catches) {
    ctx->addBlock();
    auto var = ctch.var;
    if (!ctch.var.empty()) {
      var = ctx->generateCanonicalName(ctch.var);
      ctx->add(SimplifyItem::Var, ctch.var, var);
    }
    catches.push_back({var, transformType(ctch.exc.get()), transform(ctch.suite)});
    ctx->popBlock();
  }
  resultStmt = N<TryStmt>(move(suite), move(catches), transform(stmt->finally));
}

void SimplifyVisitor::visit(ThrowStmt *stmt) {
  resultStmt = N<ThrowStmt>(transform(stmt->expr));
}

void SimplifyVisitor::visit(WithStmt *stmt) {
  assert(stmt->items.size());
  vector<StmtPtr> content;
  for (int i = int(stmt->items.size()) - 1; i >= 0; i--) {
    vector<StmtPtr> internals;
    string var =
        stmt->vars[i].empty() ? ctx->cache->getTemporaryVar("with") : stmt->vars[i];
    internals.push_back(
        N<AssignStmt>(N<IdExpr>(var), clone(stmt->items[i]), nullptr, true));
    internals.push_back(
        N<ExprStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "__enter__"))));

    internals.push_back(N<TryStmt>(
        !content.empty() ? N<SuiteStmt>(move(content), true) : clone(stmt->suite),
        vector<TryStmt::Catch>{},
        N<SuiteStmt>(N<ExprStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "__exit__"))),
                     true)));
    content = move(internals);
  }
  resultStmt = transform(N<SuiteStmt>(move(content), true));
}

void SimplifyVisitor::visit(GlobalStmt *stmt) {
  if (ctx->bases.empty() || ctx->bases.back().isType())
    error("global outside of a function");
  auto val = ctx->find(stmt->var);
  if (!val || !val->isVar())
    error("identifier '{}' not found", stmt->var);
  if (!val->getBase().empty())
    error("not a top-level variable");
  seqassert(!val->canonicalName.empty(), "'{}' does not have a canonical name",
            stmt->var);
  ctx->cache->globals.insert(val->canonicalName);
  val->global = true;
  ctx->add(SimplifyItem::Var, stmt->var, val->canonicalName, true);
}

void SimplifyVisitor::visit(ImportStmt *stmt) {
  seqassert(!ctx->inClass(), "imports within a class");
  if (stmt->from->isId("C")) {
    /// Handle C imports
    if (auto i = stmt->what->getId())
      resultStmt = transformCImport(i->value, stmt->args, stmt->ret.get(), stmt->as);
    else if (auto d = stmt->what->getDot())
      resultStmt = transformCDLLImport(d->expr.get(), d->member, stmt->args,
                                       stmt->ret.get(), stmt->as);
    else
      seqassert(false, "invalid C import statement");
    return;
  } else if (stmt->from->isId("python") && stmt->what) {
    resultStmt =
        transformPythonImport(stmt->what.get(), stmt->args, stmt->ret.get(), stmt->as);
    return;
  }

  // Transform import a.b.c.d to "a/b/c/d".
  vector<string> dirs; // Path components
  Expr *e = stmt->from.get();
  while (auto d = e->getDot()) {
    dirs.push_back(d->member);
    e = d->expr.get();
  }
  if (!e->getId() || !stmt->args.empty() || stmt->ret ||
      (stmt->what && !stmt->what->getId()))
    error("invalid import statement");
  // We have an empty stmt->from in "from .. import".
  if (!e->getId()->value.empty())
    dirs.push_back(e->getId()->value);
  // Handle dots (e.g. .. in from ..m import x).
  seqassert(stmt->dots >= 0, "negative dots in ImportStmt");
  for (int i = 0; i < stmt->dots - 1; i++)
    dirs.emplace_back("..");
  string path;
  for (int i = int(dirs.size()) - 1; i >= 0; i--)
    path += dirs[i] + (i ? "/" : "");
  // Fetch the import!
  auto file = getImportFile(ctx->cache->argv0, path, ctx->getFilename(), false,
                            ctx->cache->module0);
  if (!file)
    error("cannot locate import '{}'", join(dirs, "."));

  // If the imported file has not been seen before, load it.
  if (ctx->cache->imports.find(file->path) == ctx->cache->imports.end())
    transformNewImport(*file);
  const auto &import = ctx->cache->imports[file->path];
  string importVar = import.importVar;
  string importDoneVar = importVar + "_done";

  // Import variable is empty if it has already been loaded during the standard library
  // initialization.
  if (!ctx->isStdlibLoading && !importVar.empty()) {
    vector<StmtPtr> ifSuite;
    ifSuite.emplace_back(N<ExprStmt>(N<CallExpr>(N<IdExpr>(importVar))));
    ifSuite.emplace_back(N<UpdateStmt>(N<IdExpr>(importDoneVar), N<BoolExpr>(true)));
    resultStmt =
        N<IfStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(importDoneVar), "__invert__")),
                  N<SuiteStmt>(move(ifSuite)));
  }

  if (!stmt->what) {
    // Case 1: import foo
    ctx->add(SimplifyItem::Import, stmt->as.empty() ? path : stmt->as, file->path);
  } else if (stmt->what->isId("*")) {
    // Case 2: from foo import *
    seqassert(stmt->as.empty(), "renamed star-import");
    // Just copy all symbols from import's context here.
    for (auto &i : *(import.ctx))
      if (i.second.front().second->isGlobal()) {
        ctx->add(i.first, i.second.front().second);
        ctx->add(i.second.front().second->canonicalName, i.second.front().second);
      }
  } else {
    // Case 3: from foo import bar
    auto i = stmt->what->getId();
    seqassert(i, "not a valid import what expression");
    auto c = import.ctx->find(i->value);
    // Make sure that we are importing an existing global symbol
    if (!c || !c->isGlobal())
      error("symbol '{}' not found in {}", i->value, file->path);
    ctx->add(stmt->as.empty() ? i->value : stmt->as, c);
    ctx->add(c->canonicalName, c);
  }
}

void SimplifyVisitor::visit(FunctionStmt *stmt) {
  vector<ExprPtr> decorators;
  Attr attr = stmt->attributes;
  for (auto &d : stmt->decorators) {
    if (d->isId("__attribute__")) {
      if (stmt->decorators.size() != 1)
        error("__attribute__ cannot be mixed with other decorators");
      attr.isAttribute = true;
    } else if (d->isId(Attr::LLVM))
      attr.set(Attr::LLVM);
    else if (d->isId(Attr::Python))
      attr.set(Attr::Python);
    else if (d->isId(Attr::Internal))
      attr.set(Attr::Internal);
    else if (d->isId(Attr::Atomic))
      attr.set(Attr::Atomic);
    else if (d->isId(Attr::Property))
      attr.set(Attr::Property);
    else if (d->isId(Attr::ForceRealize))
      attr.set(Attr::ForceRealize);
    else {
      // Let's check if this is a attribute
      auto dt = transform(clone(d));
      if (dt && dt->getId()) {
        auto ci = ctx->find(dt->getId()->value);
        if (ci && ci->kind == SimplifyItem::Func) {
          if (ctx->cache->functions[ci->canonicalName].ast->attributes.isAttribute) {
            attr.set(ci->canonicalName);
            continue;
          }
        }
      }
      decorators.emplace_back(clone(d));
    }
  }
  if (attr.has(Attr::Python)) {
    // Handle Python code separately
    resultStmt = transformPythonDefinition(stmt->name, stmt->args, stmt->ret.get(),
                                           stmt->suite->firstInBlock());
    // TODO: error on decorators
    return;
  }

  auto canonicalName = ctx->generateCanonicalName(stmt->name, true);
  bool isClassMember = ctx->inClass();
  bool isEnclosedFunc = ctx->inFunction();

  if (attr.has(Attr::ForceRealize) && (ctx->getLevel() || isClassMember))
    error("builtins must be defined at the toplevel");

  auto oldBases = move(ctx->bases);
  ctx->bases = vector<SimplifyContext::Base>();
  if (!isClassMember)
    // Class members are added to class' method table
    ctx->add(SimplifyItem::Func, stmt->name, canonicalName, ctx->isToplevel());
  if (isClassMember)
    ctx->bases.push_back(oldBases[0]);
  ctx->bases.emplace_back(SimplifyContext::Base{canonicalName}); // Add new base...
  ctx->addBlock();                                               // ... and a block!
  // Set atomic flag if @atomic attribute is present.
  if (attr.has(Attr::Atomic))
    ctx->bases.back().attributes |= FLAG_ATOMIC;
  if (attr.has(Attr::Test))
    ctx->bases.back().attributes |= FLAG_TEST;
  // Add generic identifiers to the context
  vector<Param> newGenerics;
  for (auto &g : stmt->generics) {
    auto genName = ctx->generateCanonicalName(g.name);
    ctx->add(SimplifyItem::Type, g.name, genName, false, g.type != nullptr);
    if (g.type && !g.type->isId("int"))
      error("only integer static generics are supported");
    newGenerics.emplace_back(Param{genName, transformType(g.type.get()),
                                   g.deflt && g.deflt->getNone()
                                       ? clone(g.deflt)
                                       : transform(g.deflt.get(), true)});
  }
  // Parse function arguments and add them to the context.
  vector<Param> args;
  unordered_set<string> seenArgs;
  bool defaultsStarted = false, hasStarArg = false, hasKwArg = false;
  for (int ia = 0; ia < stmt->args.size(); ia++) {
    auto &a = stmt->args[ia];
    string varName = a.name;
    int stars = trimStars(varName);
    if (stars == 2) {
      if (hasKwArg || a.type || a.deflt || ia != stmt->args.size() - 1)
        error("invalid **kwargs");
      hasKwArg = true;
    } else if (stars == 1) {
      if (hasStarArg || a.type || a.deflt)
        error("invalid *args");
      hasStarArg = true;
    }

    if (in(seenArgs, varName))
      error("'{}' declared twice", varName);
    seenArgs.insert(varName);
    if (!a.deflt && defaultsStarted && !stars)
      error("non-default argument '{}' after a default argument", varName);
    defaultsStarted |= bool(a.deflt);
    auto typeAst = transformType(a.type.get());
    // If the first argument of a class method is self and if it has no type, add it.
    if (!typeAst && isClassMember && ia == 0 && a.name == "self")
      typeAst = transformType(ctx->bases[ctx->bases.size() - 2].ast.get());

    auto name = ctx->generateCanonicalName(varName);
    args.emplace_back(
        Param{string(stars, '*') + name, move(typeAst), transform(a.deflt)});
  }
  // Delay adding to context to prevent "def foo(a, b=a)"
  for (int ia = 0; ia < stmt->args.size(); ia++) {
    string varName = stmt->args[ia].name, canName = args[ia].name;
    trimStars(varName);
    trimStars(canName);
    ctx->add(SimplifyItem::Var, varName, canName);
  }
  // Parse the return type.
  if (!stmt->ret && attr.has(Attr::LLVM))
    error("LLVM functions must have a return type");
  auto ret = transformType(stmt->ret.get());
  // Parse function body.
  StmtPtr suite = nullptr;
  std::map<string, string> captures;
  if (!attr.has(Attr::Internal) && !attr.has(Attr::C)) {
    ctx->addBlock();
    if (attr.has(Attr::LLVM))
      suite = transformLLVMDefinition(stmt->suite->firstInBlock());
    else {
      if ((isEnclosedFunc || attr.has(Attr::Capture)) && !isClassMember)
        ctx->captures.emplace_back(std::map<string, string>{});
      suite = SimplifyVisitor(ctx, preamble).transform(stmt->suite);
      if (isEnclosedFunc) {
        captures = ctx->captures.back();
        ctx->captures.pop_back();
      }
    }
    ctx->popBlock();
  }

  // Once the body is done, check if this function refers to a variable (or generic)
  // from outer scope (e.g. it's parent is not -1). If so, store the name of the
  // innermost base that was referred to in this function.
  auto isMethod = ctx->bases.back().attributes & FLAG_METHOD;
  ctx->bases.pop_back();
  ctx->bases = move(oldBases);
  ctx->popBlock();
  attr.module =
      format("{}{}", ctx->moduleName.status == ImportFile::STDLIB ? "std::" : "::",
             ctx->moduleName.module);

  if (isClassMember) { // If this is a method...
    // ... set the enclosing class name...
    attr.parentClass = ctx->bases.back().name;
    // ... add the method to class' method list ...
    ctx->cache->classes[ctx->bases.back().name].methods[stmt->name].push_back(
        {canonicalName, nullptr, ctx->cache->age});
    // ... and if the function references outer class variable (by definition a
    // generic), mark it as not static as it needs fully instantiated class to be
    // realized. For example, in class A[T]: def foo(): pass, A.foo() can be realized
    // even if T is unknown. However, def bar(): return T() cannot because it needs T
    // (and is thus accordingly marked with ATTR_IS_METHOD).
    if (isMethod)
      attr.set(Attr::Method);
  }

  vector<CallExpr::Arg> partialArgs;
  if (!captures.empty()) {
    Param kw;
    if (hasKwArg) {
      kw = std::move(args.back());
      args.pop_back();
    }
    for (auto &c : captures) {
      args.emplace_back(Param{c.second, nullptr, nullptr});
      partialArgs.emplace_back(CallExpr::Arg{
          c.second, N<IdExpr>(ctx->cache->reverseIdentifierLookup[c.first])});
    }
    if (hasKwArg)
      args.emplace_back(std::move(kw));
    partialArgs.emplace_back(CallExpr::Arg{"", N<EllipsisExpr>()});
  }
  auto f = N<FunctionStmt>(canonicalName, move(ret), move(newGenerics), move(args),
                           move(suite), attr);
  // Do not clone suite: the suite will be accessed later trough the cache.
  preamble->functions.push_back(N<FunctionStmt>(canonicalName, clone(f->ret),
                                                clone_nop(f->generics),
                                                clone_nop(f->args), nullptr, attr));
  // Make sure to cache this (generic) AST for later realization.
  ctx->cache->functions[canonicalName].ast = move(f);

  ExprPtr finalExpr;
  if (!captures.empty())
    finalExpr = N<CallExpr>(N<IdExpr>(stmt->name), move(partialArgs));
  if (isClassMember && decorators.size())
    error("decorators cannot be applied to class methods");
  for (int j = int(decorators.size()) - 1; j >= 0; j--) {
    if (auto c = const_cast<CallExpr *>(decorators[j]->getCall())) {
      auto vop = move(c->args);
      vop.emplace(vop.begin(), CallExpr::Arg{"", finalExpr ? move(finalExpr)
                                                           : N<IdExpr>(stmt->name)});
      finalExpr = N<CallExpr>(move(c->expr), move(vop));
    } else {
      finalExpr = N<CallExpr>(move(decorators[j]),
                              finalExpr ? move(finalExpr) : N<IdExpr>(stmt->name));
    }
  }
  if (finalExpr)
    resultStmt = transform(N<AssignStmt>(N<IdExpr>(stmt->name), move(finalExpr)));
}

void SimplifyVisitor::visit(ClassStmt *stmt) {
  enum Magic { Init, Repr, Eq, Order, Hash, Pickle, Container, Python };
  Attr attr = stmt->attributes;
  vector<char> hasMagic(10, 2);
  hasMagic[Init] = hasMagic[Pickle] = 1;
  // @tuple(init=, repr=, eq=, order=, hash=, pickle=, container=, python=, add=,
  // internal=...)
  // @dataclass(...)
  // @extend
  for (auto &d : stmt->decorators) {
    if (auto c = d->getCall()) {
      if (c->expr->isId(Attr::Tuple))
        attr.set(Attr::Tuple);
      else if (!c->expr->isId("dataclass"))
        error("invalid class attribute");
      else if (attr.has(Attr::Tuple))
        error("class already marked as tuple");
      for (auto &a : c->args) {
        auto b = CAST(a.value, BoolExpr);
        if (!b)
          error("expected static boolean");
        auto val = b->value;
        if (a.name == "init")
          hasMagic[Init] = val;
        else if (a.name == "repr")
          hasMagic[Repr] = val;
        else if (a.name == "eq")
          hasMagic[Eq] = val;
        else if (a.name == "order")
          hasMagic[Order] = val;
        else if (a.name == "hash")
          hasMagic[Hash] = val;
        else if (a.name == "pickle")
          hasMagic[Pickle] = val;
        else if (a.name == "python")
          hasMagic[Python] = val;
        else if (a.name == "container")
          hasMagic[Container] = val;
        else
          error("invalid decorator argument");
      }
    } else if (d->isId(Attr::Tuple)) {
      if (attr.has(Attr::Tuple))
        error("class already marked as tuple");
      attr.set(Attr::Tuple);
    } else if (d->isId(Attr::Extend)) {
      attr.set(Attr::Extend);
      if (stmt->decorators.size() != 1)
        error("extend cannot be combined with other decorators");
      if (!ctx->bases.empty())
        error("extend is only allowed at the toplevel");
    } else if (d->isId(Attr::Internal)) {
      attr.set(Attr::Internal);
    }
  }
  for (int i = 1; i < hasMagic.size(); i++)
    if (hasMagic[i] == 2)
      hasMagic[i] = attr.has(Attr::Tuple) ? 1 : 0;

  // Extensions (@extend) cases are handled bit differently
  // (no auto method-generation, no arguments etc.)
  bool extension = attr.has(Attr::Extend);
  bool isRecord = attr.has(Attr::Tuple); // does it have @tuple attribute

  // Special name handling is needed because of nested classes.
  string name = stmt->name;
  if (!ctx->bases.empty() && ctx->bases.back().isType()) {
    const auto &a = ctx->bases.back().ast;
    string parentName =
        a->getId() ? a->getId()->value : a->getIndex()->expr->getId()->value;
    name = parentName + "." + name;
  }

  // Generate/find class' canonical name (unique ID) and AST
  string canonicalName;
  const ClassStmt *originalAST = nullptr;
  auto classItem =
      make_shared<SimplifyItem>(SimplifyItem::Type, "", "", ctx->isToplevel(), false);
  if (!extension) {
    classItem->canonicalName = canonicalName =
        ctx->generateCanonicalName(name, !attr.has(Attr::Internal));
    // Reference types are added to the context at this stage.
    // Record types (tuples) are added after parsing class arguments to prevent
    // recursive record types (that are allowed for reference types).
    if (!isRecord) {
      ctx->add(name, classItem);
      ctx->cache->imports[STDLIB_IMPORT].ctx->addToplevel(canonicalName, classItem);
    }
    originalAST = stmt;
  } else {
    // Find the canonical name of a class that is to be extended
    auto val = ctx->find(name);
    if (!val || val->kind != SimplifyItem::Type)
      error("cannot find type '{}' to extend", name);
    canonicalName = val->canonicalName;
    const auto &astIter = ctx->cache->classes.find(canonicalName);
    if (astIter == ctx->cache->classes.end())
      error("cannot extend type alias or an instantiation ({})", name);
    originalAST = astIter->second.ast.get();
    if (originalAST->generics.size() != stmt->generics.size())
      error("generics do not match");
  }

  // Add the class base.
  auto oldBases = move(ctx->bases);
  ctx->bases = vector<SimplifyContext::Base>();
  ctx->bases.emplace_back(SimplifyContext::Base(canonicalName));
  ctx->bases.back().ast = make_shared<IdExpr>(name);

  // Add generics, if any, to the context.
  ctx->addBlock();
  vector<ExprPtr> genAst;
  vector<Param> newGenerics;
  for (int gi = 0; gi < stmt->generics.size(); gi++) {
    if (stmt->generics[gi].deflt)
      error("default generics not supported in classes");
    if (stmt->generics[gi].type && !stmt->generics[gi].type->isId("int"))
      error("only integer static generics are supported");
    // Extension generic names might not match the original class generic names.
    // Make sure to use canonical names.
    auto genName = extension ? originalAST->generics[gi].name
                             : ctx->generateCanonicalName(stmt->generics[gi].name);
    genAst.push_back(N<IdExpr>(genName));
    // Extension generic might not have static type annotation: lift it up from the
    // original AST.
    ctx->add(SimplifyItem::Type, stmt->generics[gi].name, genName, false,
             originalAST->generics[gi].type != nullptr);
    newGenerics.emplace_back(
        Param{genName, transformType(originalAST->generics[gi].type.get()), nullptr});
  }
  ctx->bases.back().ast =
      make_shared<IndexExpr>(N<IdExpr>(name), N<TupleExpr>(move(genAst)));

  // Parse class members (arguments) and methods.
  vector<Param> args;
  auto suite = N<SuiteStmt>(vector<StmtPtr>{});
  if (!extension) {
    // Check if members are declared many times.
    unordered_set<string> seenMembers;
    for (const auto &a : stmt->args) {
      seqassert(a.type, "no type provided for '{}'", a.name);
      if (seenMembers.find(a.name) != seenMembers.end())
        error(a.type, "'{}' declared twice", a.name);
      seenMembers.insert(a.name);
      args.emplace_back(Param{a.name, transformType(a.type.get()), nullptr});
      ctx->cache->classes[canonicalName].fields.push_back({a.name, nullptr});
    }
    // Now that we are done with arguments, add record type to the context.
    // However, we need to unroll a block/base, add it, and add the unrolled
    // block/base back.
    if (isRecord) {
      ctx->addPrevBlock(name, classItem);
      ctx->cache->imports[STDLIB_IMPORT].ctx->addToplevel(canonicalName, classItem);
    }
    // Create a cached AST.
    attr.module =
        format("{}{}", ctx->moduleName.status == ImportFile::STDLIB ? "std::" : "::",
               ctx->moduleName.module);
    ctx->cache->classes[canonicalName].ast =
        N<ClassStmt>(canonicalName, move(newGenerics), move(args),
                     N<SuiteStmt>(vector<StmtPtr>()), attr);
    vector<StmtPtr> fns;
    ExprPtr codeType = ctx->bases.back().ast->clone();
    vector<string> magics{};
    // Internal classes do not get any auto-generated members.
    if (!attr.has(Attr::Internal)) {
      // Prepare a list of magics that are to be auto-generated.
      if (isRecord)
        magics = {"len", "hash"};
      else
        magics = {"new", "raw"};
      if (hasMagic[Init])
        magics.emplace_back(isRecord ? "new" : "init");
      if (hasMagic[Eq])
        for (auto &i : {"eq", "ne"})
          magics.emplace_back(i);
      if (hasMagic[Order])
        for (auto &i : {"lt", "gt", "le", "ge"})
          magics.emplace_back(i);
      if (hasMagic[Pickle])
        for (auto &i : {"pickle", "unpickle"})
          magics.emplace_back(i);
      if (hasMagic[Repr])
        magics.emplace_back("str");
      if (hasMagic[Container])
        for (auto &i : {"iter", "getitem"})
          magics.emplace_back(i);
      if (hasMagic[Python])
        for (auto &i : {"to_py", "from_py"})
          magics.emplace_back(i);

      if (hasMagic[Container] && startswith(stmt->name, TYPE_TUPLE))
        magics.emplace_back("contains");
      if (!startswith(stmt->name, TYPE_TUPLE))
        magics.emplace_back("dict");
      if (startswith(stmt->name, TYPE_TUPLE))
        magics.emplace_back("add");
    }
    // Codegen default magic methods and add them to the final AST.
    for (auto &m : magics)
      suite->stmts.push_back(transform(
          codegenMagic(m, ctx->bases.back().ast.get(), stmt->args, isRecord)));
  }
  for (auto sp : getClassMethods(stmt->suite.get()))
    if (sp && !sp->getClass())
      suite->stmts.push_back(transform(sp));
  ctx->bases.pop_back();
  ctx->bases = move(oldBases);
  ctx->popBlock();

  auto c = ctx->cache->classes[canonicalName].ast.get();
  if (!extension) {
    // Update the cached AST.
    seqassert(c, "not a class AST for {}", canonicalName);
    preamble->types.push_back(clone(ctx->cache->classes[canonicalName].ast));
    c->suite = clone(suite);
  }
  vector<StmtPtr> stmts;
  stmts.emplace_back(N<ClassStmt>(canonicalName, clone_nop(c->generics),
                                  vector<Param>{}, move(suite), Attr({Attr::Extend})));
  // Parse nested classes
  for (auto sp : getClassMethods(stmt->suite.get()))
    if (sp && sp->getClass()) {
      // Add dummy base to fix nested class' name.
      ctx->bases.emplace_back(SimplifyContext::Base(canonicalName));
      ctx->bases.back().ast = make_shared<IdExpr>(name);
      stmts.emplace_back(transform(sp));
      ctx->bases.pop_back();
    }
  resultStmt = N<SuiteStmt>(move(stmts));
}

void SimplifyVisitor::visit(CustomStmt *stmt) {
  error("invalid block: {}", stmt->head->toString());
}

/**************************************************************************************/

StmtPtr SimplifyVisitor::transformAssignment(const Expr *lhs, const Expr *rhs,
                                             const Expr *type, bool shadow,
                                             bool mustExist) {
  if (auto ei = lhs->getIndex()) {
    seqassert(!type, "unexpected type annotation");
    vector<ExprPtr> args;
    args.push_back(clone(ei->index));
    args.push_back(rhs->clone());
    return transform(N<ExprStmt>(
        N<CallExpr>(N<DotExpr>(clone(ei->expr), "__setitem__"), move(args))));
  } else if (auto ed = lhs->getDot()) {
    seqassert(!type, "unexpected type annotation");
    return N<AssignMemberStmt>(transform(ed->expr), ed->member, transform(rhs, false));
  } else if (auto e = lhs->getId()) {
    ExprPtr t = transformType(type);
    if (!shadow && !t) {
      auto val = ctx->find(e->value);
      if (val && val->isVar()) {
        if (val->getBase() == ctx->getBase())
          return N<UpdateStmt>(transform(lhs, false), transform(rhs, true),
                               !ctx->bases.empty() &&
                                   ctx->bases.back().attributes & FLAG_ATOMIC);
        else if (mustExist)
          error("variable '{}' is not global", e->value);
      }
    }
    // Function and type aliases are not normal assignments. They are treated like a
    // simple context renames.
    // Note: x = Ptr[byte] is not a simple alias, and is handled separately below.
    auto r = transform(rhs, true);
    if (r && r->getId()) {
      auto val = ctx->find(r->getId()->value);
      if (!val)
        error("cannot find '{}'", r->getId()->value);
      if (val->isType() || val->isFunc()) {
        ctx->add(e->value, val);
        return nullptr;
      }
    }
    // This assignment is a new variable assignment (not a rename or an update).
    // Generate new canonical variable name for this assignment and use it afterwards.
    auto canonical = ctx->generateCanonicalName(e->value);
    auto l = N<IdExpr>(canonical);
    bool global = ctx->isToplevel();
    // ctx->moduleName != MODULE_MAIN;
    // ⚠️ TODO: should we make __main__ top-level variables NOT global by default?
    // Problem: a = [1]; def foo(): a.append(2) won't work anymore as in Python.
    if (global)
      ctx->cache->globals.insert(canonical);
    // Handle type aliases as well!
    ctx->add(r && r->isType() ? SimplifyItem::Type : SimplifyItem::Var, e->value,
             canonical, global);
    if (ctx->isToplevel()) {
      if (r && r->isType()) {
        preamble->types.push_back(N<AssignStmt>(N<IdExpr>(canonical), clone(r)));
      } else {
        preamble->globals.push_back(
            N<AssignStmt>(N<IdExpr>(canonical), nullptr, move(t)));
        return r ? N<UpdateStmt>(move(l), move(r)) : nullptr;
      }
    }
    return N<AssignStmt>(move(l), move(r), move(t));
  } else {
    error("invalid assignment");
    return nullptr;
  }
}

void SimplifyVisitor::unpackAssignments(const Expr *lhs, const Expr *rhs,
                                        vector<StmtPtr> &stmts, bool shadow,
                                        bool mustExist) {
  vector<Expr *> leftSide;
  if (auto et = lhs->getTuple()) { // (a, b) = ...
    for (auto &i : et->items)
      leftSide.push_back(i.get());
  } else if (auto el = lhs->getList()) { // [a, b] = ...
    for (auto &i : el->items)
      leftSide.push_back(i.get());
  } else { // A simple assignment.
    stmts.push_back(transformAssignment(lhs, rhs, nullptr, shadow, mustExist));
    return;
  }

  // Prepare the right-side expression
  auto srcPos = rhs;
  ExprPtr newRhs = nullptr; // This expression must not be deleted until the very end.
  if (!rhs->getId()) { // Store any non-trivial right-side expression (assign = rhs).
    auto var = ctx->cache->getTemporaryVar("assign");
    newRhs = Nx<IdExpr>(srcPos, var);
    stmts.push_back(transformAssignment(newRhs.get(), rhs, nullptr, shadow, mustExist));
    rhs = newRhs.get();
  }

  // Process each assignment until the fist StarExpr (if any).
  int st;
  for (st = 0; st < leftSide.size(); st++) {
    if (leftSide[st]->getStar())
      break;
    // Transformation: leftSide_st = rhs[st]
    auto rightSide = Nx<IndexExpr>(srcPos, rhs->clone(), Nx<IntExpr>(srcPos, st));
    // Recursively process the assignment (as we can have cases like (a, (b, c)) = d).
    unpackAssignments(leftSide[st], rightSide.get(), stmts, shadow, mustExist);
  }
  // If there is a StarExpr, process it and the remaining assignments after it (if
  // any).
  if (st < leftSide.size() && leftSide[st]->getStar()) {
    // StarExpr becomes SliceExpr: in (a, *b, c) = d, b is d[1:-2]
    auto rightSide = Nx<IndexExpr>(
        srcPos, rhs->clone(),
        Nx<SliceExpr>(srcPos, Nx<IntExpr>(srcPos, st),
                      // This slice is either [st:] or [st:-lhs_len + st + 1]
                      leftSide.size() == st + 1
                          ? nullptr
                          : Nx<IntExpr>(srcPos, -leftSide.size() + st + 1),
                      nullptr));
    unpackAssignments(leftSide[st]->getStar()->what.get(), rightSide.get(), stmts,
                      shadow, mustExist);
    st += 1;
    // Keep going till the very end. Remaining assignments use negative indices (-1,
    // -2 etc) as we are not sure how big is StarExpr.
    for (; st < leftSide.size(); st++) {
      if (leftSide[st]->getStar())
        error(leftSide[st], "multiple unpack expressions");
      rightSide = Nx<IndexExpr>(srcPos, rhs->clone(),
                                Nx<IntExpr>(srcPos, -leftSide.size() + st));
      unpackAssignments(leftSide[st], rightSide.get(), stmts, shadow, mustExist);
    }
  }
}

StmtPtr SimplifyVisitor::transformPattern(ExprPtr var, ExprPtr pattern, StmtPtr suite) {
  auto isinstance = [&](const ExprPtr &e, const string &typ) -> ExprPtr {
    return N<CallExpr>(N<IdExpr>("isinstance"), e->clone(), N<IdExpr>(typ));
  };
  auto findEllipsis = [&](const vector<ExprPtr> &items) {
    int i = items.size();
    for (int it = 0; it < items.size(); it++)
      if (items[it]->getEllipsis()) {
        if (i != items.size())
          error("cannot have multiple ranges in a pattern");
        i = it;
      }
    return i;
  };

  if (pattern->getInt() || CAST(pattern, BoolExpr)) {
    return N<IfStmt>(
        isinstance(var, pattern->getInt() ? "int" : "bool"),
        N<IfStmt>(N<BinaryExpr>(var->clone(), "==", move(pattern)), move(suite)));
  } else if (auto er = CAST(pattern, RangeExpr)) {
    return N<IfStmt>(
        isinstance(var, "int"),
        N<IfStmt>(N<BinaryExpr>(var->clone(), ">=", clone(er->start)),
                  N<IfStmt>(N<BinaryExpr>(var->clone(), "<=", clone(er->stop)),
                            move(suite))));
  } else if (auto et = pattern->getTuple()) {
    for (int it = int(et->items.size()) - 1; it >= 0; it--)
      suite = transformPattern(N<IndexExpr>(var->clone(), N<IntExpr>(it)),
                               clone(et->items[it]), move(suite));
    return N<IfStmt>(
        isinstance(var, "Tuple"),
        N<IfStmt>(N<BinaryExpr>(N<CallExpr>(N<IdExpr>("staticlen"), clone(var)),
                                "==", N<IntExpr>(et->items.size())),
                  move(suite)));
  } else if (auto el = pattern->getList()) {
    auto ellipsis = findEllipsis(el->items), sz = int(el->items.size());
    string op;
    if (ellipsis == el->items.size())
      op = "==";
    else
      op = ">=", sz -= 1;
    for (int it = int(el->items.size()) - 1; it > ellipsis; it--)
      suite = transformPattern(
          N<IndexExpr>(var->clone(), N<IntExpr>(it - el->items.size())),
          clone(el->items[it]), move(suite));
    for (int it = ellipsis - 1; it >= 0; it--)
      suite = transformPattern(N<IndexExpr>(var->clone(), N<IntExpr>(it)),
                               clone(el->items[it]), move(suite));
    return N<IfStmt>(isinstance(var, "List"),
                     N<IfStmt>(N<BinaryExpr>(N<CallExpr>(N<IdExpr>("len"), clone(var)),
                                             op, N<IntExpr>(sz)),
                               move(suite)));
  } else if (auto eb = pattern->getBinary()) {
    if (eb->op == "|") {
      return N<SuiteStmt>(transformPattern(clone(var), clone(eb->lexpr), clone(suite)),
                          transformPattern(clone(var), clone(eb->rexpr), move(suite)));
    }
  } else if (auto ea = CAST(pattern, AssignExpr)) {
    seqassert(ea->var->getId(), "only simple assignment expressions are supported");
    return N<SuiteStmt>(N<AssignStmt>(clone(ea->var), clone(var)),
                        transformPattern(clone(var), clone(ea->expr), clone(suite)),
                        true);
  } else if (auto ei = pattern->getId()) {
    if (ei->value != "_")
      return N<SuiteStmt>(N<AssignStmt>(clone(pattern), clone(var)), move(suite), true);
    else
      return suite;
  }
  pattern = transform(pattern); // basically check for errors
  return N<IfStmt>(
      N<CallExpr>(N<IdExpr>("hasattr"), N<TypeOfExpr>(var->clone()),
                  N<StringExpr>("__match__")),
      N<IfStmt>(N<CallExpr>(N<DotExpr>(var->clone(), "__match__"), move(pattern)),
                move(suite)));
}

StmtPtr SimplifyVisitor::transformCImport(const string &name, const vector<Param> &args,
                                          const Expr *ret, const string &altName) {
  vector<Param> fnArgs;
  for (int ai = 0; ai < args.size(); ai++) {
    seqassert(args[ai].name.empty(), "unexpected argument name");
    seqassert(!args[ai].deflt, "unexpected default argument");
    seqassert(args[ai].type, "missing type");
    fnArgs.emplace_back(Param{args[ai].name.empty() ? format("a{}", ai) : args[ai].name,
                              args[ai].type->clone(), nullptr});
  }
  auto f = N<FunctionStmt>(name, ret ? ret->clone() : N<IdExpr>("void"),
                           vector<Param>(), move(fnArgs), nullptr, Attr({Attr::C}));
  StmtPtr tf = transform(f.get()); // Already in the preamble
  if (!altName.empty())
    ctx->add(altName, ctx->find(name));
  return tf;
}

StmtPtr SimplifyVisitor::transformCDLLImport(const Expr *dylib, const string &name,
                                             const vector<Param> &args, const Expr *ret,
                                             const string &altName) {
  vector<StmtPtr> stmts;
  // fptr = _dlsym(dylib, "name")
  stmts.push_back(
      N<AssignStmt>(N<IdExpr>("fptr"), N<CallExpr>(N<IdExpr>("_dlsym"), dylib->clone(),
                                                   N<StringExpr>(name))));
  // Prepare Function[args...]
  vector<ExprPtr> fnArgs;
  fnArgs.emplace_back(N<ListExpr>(vector<ExprPtr>{}));
  fnArgs.emplace_back(ret ? ret->clone() : N<IdExpr>("void"));
  for (const auto &a : args) {
    seqassert(a.name.empty(), "unexpected argument name");
    seqassert(!a.deflt, "unexpected default argument");
    seqassert(a.type, "missing type");
    const_cast<ListExpr *>(fnArgs[0]->getList())->items.emplace_back(clone(a.type));
  }
  // f = Function[args...](fptr)
  stmts.emplace_back(N<AssignStmt>(
      N<IdExpr>("f"),
      N<CallExpr>(N<IndexExpr>(N<IdExpr>("Function"), N<TupleExpr>(move(fnArgs))),
                  N<IdExpr>("fptr"))));
  // Check if a return type is void or not
  bool isVoid = !ret || (ret->getId() && ret->getId()->value == "void");
  fnArgs.clear();
  for (int i = 0; i < args.size(); i++)
    fnArgs.emplace_back(N<IdExpr>(format("a{}", i)));
  // f(args...)
  auto call = N<CallExpr>(N<IdExpr>("f"), move(fnArgs));
  if (!isVoid)
    stmts.push_back(N<ReturnStmt>(move(call)));
  else
    stmts.push_back(N<ExprStmt>(move(call)));
  vector<Param> params;
  // Prepare final FunctionStmt and transform it
  for (int i = 0; i < args.size(); i++)
    params.emplace_back(Param{format("a{}", i), clone(args[i].type)});
  return transform(N<FunctionStmt>(altName.empty() ? name : altName,
                                   ret ? ret->clone() : nullptr, vector<Param>(),
                                   move(params), N<SuiteStmt>(move(stmts))));
}

StmtPtr SimplifyVisitor::transformPythonImport(const Expr *what,
                                               const vector<Param> &args,
                                               const Expr *ret, const string &altName) {
  // Get a module name (e.g. os.path)
  vector<string> dirs;
  auto e = what;
  while (auto d = e->getDot()) {
    dirs.push_back(d->member);
    e = d->expr.get();
  }
  seqassert(e && e->getId(), "invalid import python statement");
  dirs.push_back(e->getId()->value);
  string name = dirs[0], lib;
  for (int i = int(dirs.size()) - 1; i > 0; i--)
    lib += dirs[i] + (i > 1 ? "." : "");

  // Simple module import: from python import foo
  if (!ret && args.empty())
    // altName = pyobj._import("name")
    return transform(N<AssignStmt>(
        N<IdExpr>(altName.empty() ? name : altName),
        N<CallExpr>(N<DotExpr>(N<IdExpr>("pyobj"), "_import"),
                    N<StringExpr>((lib.empty() ? "" : lib + ".") + name))));

  // Typed function import: from python import foo.bar(int) -> float.
  // f = pyobj._import("lib")._getattr("name")
  auto call = N<AssignStmt>(
      N<IdExpr>("f"),
      N<CallExpr>(N<DotExpr>(N<CallExpr>(N<DotExpr>(N<IdExpr>("pyobj"), "_import"),
                                         N<StringExpr>(lib)),
                             "_getattr"),
                  N<StringExpr>(name)));
  // Make a call expression: f(args...)
  vector<Param> params;
  vector<ExprPtr> callArgs;
  for (int i = 0; i < args.size(); i++) {
    params.emplace_back(Param{format("a{}", i), clone(args[i].type), nullptr});
    callArgs.emplace_back(N<IdExpr>(format("a{}", i)));
  }
  // Make a return expression: return f(args...),
  // or return retType.__from_py__(f(args...))
  ExprPtr retExpr = N<CallExpr>(N<IdExpr>("f"), move(callArgs));
  if (ret && !ret->isId("void"))
    retExpr = N<CallExpr>(N<DotExpr>(ret->clone(), "__from_py__"), move(retExpr));
  StmtPtr retStmt = nullptr;
  if (ret && ret->isId("void"))
    retStmt = N<ExprStmt>(move(retExpr));
  else
    retStmt = N<ReturnStmt>(move(retExpr));
  // Return a wrapper function
  return transform(N<FunctionStmt>(
      altName.empty() ? name : altName, ret ? ret->clone() : nullptr, vector<Param>(),
      move(params), N<SuiteStmt>(move(call), move(retStmt))));
}

void SimplifyVisitor::transformNewImport(const ImportFile &file) {
  // Use a clean context to parse a new file.
  if (ctx->cache->age)
    ctx->cache->age++;
  auto ictx = make_shared<SimplifyContext>(file.path, ctx->cache);
  ictx->isStdlibLoading = ctx->isStdlibLoading;
  ictx->moduleName = file;
  auto import = ctx->cache->imports.insert({file.path, {file.path, ictx}}).first;
  StmtPtr sf = parseFile(file.path);
  auto sn = SimplifyVisitor(ictx, preamble).transform(sf);

  // If we are loading standard library, we won't wrap imports in functions as we
  // assume that standard library has no recursive imports. We will just append the
  // top-level statements as-is.
  if (ctx->isStdlibLoading) {
    resultStmt = N<SuiteStmt>(move(sn), true);
  } else {
    // Generate import function identifier.
    string importVar = import->second.importVar =
               ctx->cache->getTemporaryVar("import", '.'),
           importDoneVar;
    // import_done = False (global variable that indicates if an import has been
    // loaded)
    preamble->globals.push_back(N<AssignStmt>(
        N<IdExpr>(importDoneVar = importVar + "_done"), N<BoolExpr>(false)));
    ctx->cache->globals.insert(importDoneVar);
    vector<StmtPtr> stmts;
    stmts.push_back(nullptr);
    // __name__ = <import name> (set the Python's __name__ variable)
    stmts.push_back(
        N<AssignStmt>(N<IdExpr>("__name__"), N<StringExpr>(ictx->moduleName.module)));
    vector<string> globalVars;
    // We need to wrap all imported top-level statements (not signatures! they have
    // already been handled and are in the preamble) into a function. We also take the
    // list of global variables so that we can access them via "global" statement.
    auto processStmt = [&](StmtPtr &s) {
      if (s->getAssign() && s->getAssign()->lhs->getId()) { // a = ... globals
        auto a = const_cast<AssignStmt *>(s->getAssign());
        auto val = ictx->find(a->lhs->getId()->value);
        seqassert(val, "cannot locate '{}' in imported file {}",
                  s->getAssign()->lhs->getId()->value, file.path);
        if (val->kind == SimplifyItem::Var && val->global && val->base.empty()) {
          globalVars.emplace_back(val->canonicalName);
          stmts.push_back(N<UpdateStmt>(move(a->lhs), move(a->rhs)));
        } else {
          stmts.push_back(move(s));
        }
      } else if (!s->getFunction() && !s->getClass()) {
        stmts.push_back(move(s));
      }
    };
    if (auto st = const_cast<SuiteStmt *>(sn->getSuite()))
      for (auto &ss : st->stmts)
        processStmt(ss);
    else
      processStmt(sn);
    stmts[0] = N<SuiteStmt>();
    for (auto &g : globalVars)
      const_cast<SuiteStmt *>(stmts[0]->getSuite())->stmts.push_back(N<GlobalStmt>(g));
    // Add a def import(): ... manually to the cache and to the preamble (it won't be
    // transformed here!) and set ATTR_FORCE_REALIZE to realize it during the
    // type-checking even if it is not called.
    ctx->cache->functions[importVar].ast =
        N<FunctionStmt>(importVar, nullptr, vector<Param>{}, vector<Param>{},
                        N<SuiteStmt>(move(stmts)), Attr({Attr::ForceRealize}));
    preamble->functions.push_back(N<FunctionStmt>(importVar, nullptr, vector<Param>{},
                                                  vector<Param>{}, nullptr,
                                                  Attr({Attr::ForceRealize})));
  }
}

StmtPtr SimplifyVisitor::transformPythonDefinition(const string &name,
                                                   const vector<Param> &args,
                                                   const Expr *ret,
                                                   const Stmt *codeStmt) {
  seqassert(codeStmt && codeStmt->getExpr() && codeStmt->getExpr()->expr->getString(),
            "invalid Python definition");
  auto code = codeStmt->getExpr()->expr->getString()->value;
  vector<string> pyargs;
  for (const auto &a : args)
    pyargs.emplace_back(a.name);
  code = format("def {}({}):\n{}\n", name, join(pyargs, ", "), code);
  return transform(N<SuiteStmt>(
      N<ExprStmt>(
          N<CallExpr>(N<DotExpr>(N<IdExpr>("pyobj"), "_exec"), N<StringExpr>(code))),
      N<ImportStmt>(N<IdExpr>("python"), N<DotExpr>(N<IdExpr>("__main__"), name),
                    clone_nop(args), ret ? ret->clone() : N<IdExpr>("pyobj"))));
}

StmtPtr SimplifyVisitor::transformLLVMDefinition(const Stmt *codeStmt) {
  seqassert(codeStmt && codeStmt->getExpr() && codeStmt->getExpr()->expr->getString(),
            "invalid LLVM definition");

  auto code = codeStmt->getExpr()->expr->getString()->value;
  vector<StmtPtr> items;
  auto se = N<StringExpr>("");
  string &finalCode = se->value;
  items.push_back(N<ExprStmt>(move(se)));

  int braceCount = 0, braceStart = 0;
  for (int i = 0; i < code.size(); i++) {
    if (i < code.size() - 1 && code[i] == '{' && code[i + 1] == '=') {
      if (braceStart < i)
        finalCode += escapeFStringBraces(code, braceStart, i - braceStart) + '{';
      if (!braceCount) {
        braceStart = i + 2;
        braceCount++;
      } else {
        error("invalid LLVM substitution");
      }
    } else if (braceCount && code[i] == '}') {
      braceCount--;
      string exprCode = code.substr(braceStart, i - braceStart);
      auto offset = getSrcInfo();
      offset.col += i;
      auto expr = transform(parseExpr(exprCode, offset).get(), true);
      if (!expr->isType() && !expr->isStaticExpr)
        error(expr, "not a type or static expression", expr->toString());
      items.push_back(N<ExprStmt>(move(expr)));
      braceStart = i + 1;
      finalCode += '}';
    }
  }
  if (braceCount)
    error("invalid LLVM substitution");
  if (braceStart != code.size())
    finalCode += escapeFStringBraces(code, braceStart, int(code.size()) - braceStart);
  return N<SuiteStmt>(move(items));
}

StmtPtr SimplifyVisitor::codegenMagic(const string &op, const Expr *typExpr,
                                      const vector<Param> &args, bool isRecord) {
#define I(s) N<IdExpr>(s)
  assert(typExpr);
  ExprPtr ret;
  vector<Param> fargs;
  vector<StmtPtr> stmts;
  Attr attr;
  if (op == "new") {
    // Classes: @internal def __new__() -> T
    // Tuples: @internal def __new__(a1: T1, ..., aN: TN) -> T
    ret = typExpr->clone();
    if (isRecord)
      for (auto &a : args)
        fargs.emplace_back(
            Param{a.name, clone(a.type),
                  a.deflt ? clone(a.deflt) : N<CallExpr>(clone(a.type))});
    attr.set(Attr::Internal);
  } else if (op == "init") {
    // Classes: def __init__(self: T, a1: T1, ..., aN: TN) -> void:
    //            self.aI = aI ...
    ret = I("void");
    fargs.emplace_back(Param{"self", typExpr->clone()});
    for (auto &a : args) {
      stmts.push_back(N<AssignStmt>(N<DotExpr>(I("self"), a.name), I(a.name)));
      fargs.emplace_back(Param{a.name, clone(a.type),
                               a.deflt ? clone(a.deflt) : N<CallExpr>(clone(a.type))});
    }
  } else if (op == "raw") {
    // Classes: def __raw__(self: T) -> Ptr[byte]:
    //            return __internal__.class_raw(self)
    fargs.emplace_back(Param{"self", typExpr->clone()});
    ret = N<IndexExpr>(N<IdExpr>("Ptr"), N<IdExpr>("byte"));
    stmts.emplace_back(N<ReturnStmt>(
        N<CallExpr>(N<DotExpr>(I("__internal__"), "class_raw"), I("self"))));
  } else if (op == "getitem") {
    // Tuples: def __getitem__(self: T, index: int) -> T1:
    //           return __internal__.tuple_getitem[T, T1](self, index)
    //         (error during a realizeFunc() method if T is a heterogeneous tuple)
    fargs.emplace_back(Param{"self", typExpr->clone()});
    fargs.emplace_back(Param{"index", I("int")});
    ret = !args.empty() ? clone(args[0].type) : I("void");
    vector<ExprPtr> idxArgs;
    idxArgs.emplace_back(typExpr->clone());
    idxArgs.emplace_back(ret->clone());
    stmts.emplace_back(N<ReturnStmt>(
        N<CallExpr>(N<IndexExpr>(N<DotExpr>(I("__internal__"), "tuple_getitem"),
                                 N<TupleExpr>(move(idxArgs))),
                    I("self"), I("index"))));
  } else if (op == "iter") {
    // Tuples: def __iter__(self: T) -> Generator[T]:
    //           yield self.aI ...
    //         (error during a realizeFunc() method if T is a heterogeneous tuple)
    fargs.emplace_back(Param{"self", typExpr->clone()});
    ret = N<IndexExpr>(I("Generator"), !args.empty() ? clone(args[0].type) : I("int"));
    for (auto &a : args)
      stmts.emplace_back(N<YieldStmt>(N<DotExpr>(N<IdExpr>("self"), a.name)));
    if (args.empty()) // Hack for empty tuple: yield from List[int]()
      stmts.emplace_back(
          N<YieldFromStmt>(N<CallExpr>(N<IndexExpr>(I("List"), I("int")))));
  } else if (op == "contains") {
    // Tuples: def __contains__(self: T, what) -> bool:
    //            if isinstance(what, T1): if what == self.a1: return True ...
    //            return False
    fargs.emplace_back(Param{"self", typExpr->clone()});
    fargs.emplace_back(Param{"what", nullptr});
    ret = I("bool");
    for (auto &a : args)
      stmts.push_back(N<IfStmt>(N<CallExpr>(I("isinstance"), I("what"), clone(a.type)),
                                N<IfStmt>(N<CallExpr>(N<DotExpr>(I("what"), "__eq__"),
                                                      N<DotExpr>(I("self"), a.name)),
                                          N<ReturnStmt>(N<BoolExpr>(true)))));
    stmts.emplace_back(N<ReturnStmt>(N<BoolExpr>(false)));
  } else if (op == "eq") {
    // def __eq__(self: T, other: T) -> bool:
    //   if not self.arg1.__eq__(other.arg1): return False ...
    //   return True
    fargs.emplace_back(Param{"self", typExpr->clone()});
    fargs.emplace_back(Param{"other", typExpr->clone()});
    ret = I("bool");
    for (auto &a : args)
      stmts.push_back(N<IfStmt>(
          N<UnaryExpr>("!",
                       N<CallExpr>(N<DotExpr>(N<DotExpr>(I("self"), a.name), "__eq__"),
                                   N<DotExpr>(I("other"), a.name))),
          N<ReturnStmt>(N<BoolExpr>(false))));
    stmts.emplace_back(N<ReturnStmt>(N<BoolExpr>(true)));
  } else if (op == "ne") {
    // def __ne__(self: T, other: T) -> bool:
    //   if self.arg1.__ne__(other.arg1): return True ...
    //   return False
    fargs.emplace_back(Param{"self", typExpr->clone()});
    fargs.emplace_back(Param{"other", typExpr->clone()});
    ret = I("bool");
    for (auto &a : args)
      stmts.emplace_back(
          N<IfStmt>(N<CallExpr>(N<DotExpr>(N<DotExpr>(I("self"), a.name), "__ne__"),
                                N<DotExpr>(I("other"), a.name)),
                    N<ReturnStmt>(N<BoolExpr>(true))));
    stmts.push_back(N<ReturnStmt>(N<BoolExpr>(false)));
  } else if (op == "lt" || op == "gt") {
    // def __lt__(self: T, other: T) -> bool:  (same for __gt__)
    //   if self.arg1.__lt__(other.arg1): return True
    //   elif self.arg1.__eq__(other.arg1):
    //      ... (arg2, ...) ...
    //   return False
    fargs.emplace_back(Param{"self", typExpr->clone()});
    fargs.emplace_back(Param{"other", typExpr->clone()});
    ret = I("bool");
    vector<StmtPtr> *v = &stmts;
    for (int i = 0; i < (int)args.size() - 1; i++) {
      v->emplace_back(N<IfStmt>(
          N<CallExpr>(
              N<DotExpr>(N<DotExpr>(I("self"), args[i].name), format("__{}__", op)),
              N<DotExpr>(I("other"), args[i].name)),
          N<ReturnStmt>(N<BoolExpr>(true)),
          N<CallExpr>(N<DotExpr>(N<DotExpr>(I("self"), args[i].name), "__eq__"),
                      N<DotExpr>(I("other"), args[i].name)),
          N<SuiteStmt>()));
      v = &((SuiteStmt *)(((IfStmt *)(v->back().get()))->ifs.back().suite).get())
               ->stmts;
    }
    if (!args.empty())
      v->emplace_back(N<ReturnStmt>(N<CallExpr>(
          N<DotExpr>(N<DotExpr>(I("self"), args.back().name), format("__{}__", op)),
          N<DotExpr>(I("other"), args.back().name))));
    stmts.emplace_back(N<ReturnStmt>(N<BoolExpr>(false)));
  } else if (op == "le" || op == "ge") {
    // def __le__(self: T, other: T) -> bool:  (same for __ge__)
    //   if not self.arg1.__le__(other.arg1): return False
    //   elif self.arg1.__eq__(other.arg1):
    //      ... (arg2, ...) ...
    //   return True
    fargs.emplace_back(Param{"self", typExpr->clone()});
    fargs.emplace_back(Param{"other", typExpr->clone()});
    ret = I("bool");
    vector<StmtPtr> *v = &stmts;
    for (int i = 0; i < (int)args.size() - 1; i++) {
      v->emplace_back(N<IfStmt>(
          N<UnaryExpr>("!", N<CallExpr>(N<DotExpr>(N<DotExpr>(I("self"), args[i].name),
                                                   format("__{}__", op)),
                                        N<DotExpr>(I("other"), args[i].name))),
          N<ReturnStmt>(N<BoolExpr>(false)),
          N<CallExpr>(N<DotExpr>(N<DotExpr>(I("self"), args[i].name), "__eq__"),
                      N<DotExpr>(I("other"), args[i].name)),
          N<SuiteStmt>()));
      v = &((SuiteStmt *)(((IfStmt *)(v->back().get()))->ifs.back().suite).get())
               ->stmts;
    }
    if (!args.empty())
      v->emplace_back(N<ReturnStmt>(N<CallExpr>(
          N<DotExpr>(N<DotExpr>(I("self"), args.back().name), format("__{}__", op)),
          N<DotExpr>(I("other"), args.back().name))));
    stmts.emplace_back(N<ReturnStmt>(N<BoolExpr>(true)));
  } else if (op == "hash") {
    // def __hash__(self: T) -> int:
    //   seed = 0
    //   seed = (
    //     seed ^ ((self.arg1.__hash__() + 2654435769) + ((seed << 6) + (seed >> 2)))
    //   ) ...
    //   return seed
    fargs.emplace_back(Param{"self", typExpr->clone()});
    ret = I("int");
    stmts.emplace_back(N<AssignStmt>(I("seed"), N<IntExpr>(0)));
    for (auto &a : args)
      stmts.push_back(N<AssignStmt>(
          I("seed"),
          N<BinaryExpr>(
              I("seed"), "^",
              N<BinaryExpr>(
                  N<BinaryExpr>(N<CallExpr>(N<DotExpr>(N<DotExpr>(I("self"), a.name),
                                                       "__hash__")),
                                "+", N<IntExpr>(0x9e3779b9)),
                  "+",
                  N<BinaryExpr>(N<BinaryExpr>(I("seed"), "<<", N<IntExpr>(6)), "+",
                                N<BinaryExpr>(I("seed"), ">>", N<IntExpr>(2)))))));
    stmts.emplace_back(N<ReturnStmt>(I("seed")));
  } else if (op == "pickle") {
    // def __pickle__(self: T, dest: Ptr[byte]) -> void:
    //   self.arg1.__pickle__(dest) ...
    fargs.emplace_back(Param{"self", typExpr->clone()});
    fargs.emplace_back(
        Param{"dest", N<IndexExpr>(N<IdExpr>("Ptr"), N<IdExpr>("byte"))});
    ret = I("void");
    for (auto &a : args)
      stmts.emplace_back(N<ExprStmt>(N<CallExpr>(
          N<DotExpr>(N<DotExpr>(I("self"), a.name), "__pickle__"), I("dest"))));
  } else if (op == "unpickle") {
    // def __unpickle__(src: Ptr[byte]) -> T:
    //   return T(T1.__unpickle__(src),...)
    fargs.emplace_back(Param{"src", N<IndexExpr>(N<IdExpr>("Ptr"), N<IdExpr>("byte"))});
    ret = typExpr->clone();
    vector<CallExpr::Arg> ar;
    for (auto &a : args)
      ar.emplace_back(CallExpr::Arg{
          "", N<CallExpr>(N<DotExpr>(clone(a.type), "__unpickle__"), I("src"))});
    stmts.emplace_back(N<ReturnStmt>(N<CallExpr>(typExpr->clone(), move(ar))));
  } else if (op == "len") {
    // def __len__(self: T) -> int:
    //   return N (number of args)
    fargs.emplace_back(Param{"self", typExpr->clone()});
    ret = I("int");
    stmts.emplace_back(N<ReturnStmt>(N<IntExpr>(args.size())));
  } else if (op == "to_py") {
    // def __to_py__(self: T) -> pyobj:
    //   o = pyobj._tuple_new(N)  (number of args)
    //   o._tuple_set(1, self.arg1.__to_py__()) ...
    //   return o
    fargs.emplace_back(Param{"self", typExpr->clone()});
    ret = I("pyobj");
    stmts.emplace_back(
        N<AssignStmt>(I("o"), N<CallExpr>(N<DotExpr>(I("pyobj"), "_tuple_new"),
                                          N<IntExpr>(args.size()))));
    for (int i = 0; i < args.size(); i++)
      stmts.push_back(N<ExprStmt>(N<CallExpr>(
          N<DotExpr>(I("o"), "_tuple_set"), N<IntExpr>(i),
          N<CallExpr>(N<DotExpr>(N<DotExpr>(I("self"), args[i].name), "__to_py__")))));
    stmts.emplace_back(N<ReturnStmt>(I("o")));
  } else if (op == "from_py") {
    // def __from_py__(src: pyobj) -> T:
    //   return T(T1.__from_py__(src._tuple_get(1)), ...)
    fargs.emplace_back(Param{"src", I("pyobj")});
    ret = typExpr->clone();
    vector<CallExpr::Arg> ar;
    for (int i = 0; i < args.size(); i++)
      ar.push_back(CallExpr::Arg{
          "",
          N<CallExpr>(N<DotExpr>(clone(args[i].type), "__from_py__"),
                      N<CallExpr>(N<DotExpr>(I("src"), "_tuple_get"), N<IntExpr>(i)))});
    stmts.emplace_back(N<ReturnStmt>(N<CallExpr>(typExpr->clone(), move(ar))));
  } else if (op == "str") {
    // def __str__(self: T) -> str:
    //   a = __array__[str](N)  (number of args)
    //   n = __array__[str](N)  (number of args)
    //   a.__setitem__(0, self.arg1.__str__()) ...
    //   n.__setitem__(0, "arg1") ...  (if not a Tuple.N; otherwise "")
    //   return __internal__.tuple_str(a.ptr, n.ptr, N)
    fargs.emplace_back(Param{"self", typExpr->clone()});
    ret = I("str");
    if (!args.empty()) {
      stmts.emplace_back(
          N<AssignStmt>(I("a"), N<CallExpr>(N<IndexExpr>(I("__array__"), I("str")),
                                            N<IntExpr>(args.size()))));
      stmts.emplace_back(
          N<AssignStmt>(I("n"), N<CallExpr>(N<IndexExpr>(I("__array__"), I("str")),
                                            N<IntExpr>(args.size()))));
      for (int i = 0; i < args.size(); i++) {
        stmts.push_back(N<ExprStmt>(N<CallExpr>(
            N<DotExpr>(I("a"), "__setitem__"), N<IntExpr>(i),
            N<CallExpr>(N<DotExpr>(N<DotExpr>(I("self"), args[i].name), "__str__")))));

        auto name = typExpr->getIndex() ? typExpr->getIndex()->expr->getId() : nullptr;
        stmts.push_back(N<ExprStmt>(N<CallExpr>(
            N<DotExpr>(I("n"), "__setitem__"), N<IntExpr>(i),
            N<StringExpr>(name && !startswith(name->value, TYPE_TUPLE) ? args[i].name
                                                                       : ""))));
      }
      stmts.emplace_back(N<ReturnStmt>(N<CallExpr>(
          N<DotExpr>(I("__internal__"), "tuple_str"), N<DotExpr>(I("a"), "ptr"),
          N<DotExpr>(I("n"), "ptr"), N<IntExpr>(args.size()))));
    } else {
      stmts.emplace_back(N<ReturnStmt>(N<StringExpr>("()")));
    }
  } else if (op == "dict") {
    // def __dict__(self: T):
    //   d = List[str](N)
    //   d.append('arg1')  ...
    //   return d
    fargs.emplace_back(Param{"self", typExpr->clone()});
    stmts.emplace_back(
        N<AssignStmt>(I("d"), N<CallExpr>(N<IndexExpr>(I("List"), I("str")),
                                          N<IntExpr>(args.size()))));
    for (auto &a : args)
      stmts.push_back(N<ExprStmt>(
          N<CallExpr>(N<DotExpr>(I("d"), "append"), N<StringExpr>(a.name))));
    stmts.emplace_back(N<ReturnStmt>(I("d")));
  } else if (op == "add") {
    // def __add__(self, tup):
    //   return (*self, *t)
    fargs.emplace_back(Param{"self", typExpr->clone()});
    fargs.emplace_back(Param{"tup", nullptr});
    vector<ExprPtr> ta;
    ta.emplace_back(N<StarExpr>(N<IdExpr>("self")));
    ta.emplace_back(N<StarExpr>(N<IdExpr>("tup")));
    stmts.emplace_back(N<ReturnStmt>(N<TupleExpr>(move(ta))));
  } else {
    seqassert(false, "invalid magic {}", op);
  }
#undef I
  auto t = make_unique<FunctionStmt>(format("__{}__", op), move(ret), vector<Param>{},
                                     move(fargs), N<SuiteStmt>(move(stmts)), attr);
  t->setSrcInfo(ctx->cache->generateSrcInfo());
  return t;
}

vector<const Stmt *> SimplifyVisitor::getClassMethods(const Stmt *s) {
  vector<const Stmt *> v;
  if (!s)
    return v;
  if (auto sp = s->getSuite()) {
    for (const auto &ss : sp->stmts)
      for (auto u : getClassMethods(ss.get()))
        v.push_back(u);
  } else if (s->getExpr() && s->getExpr()->expr->getString()) {
    /// Those are doc-strings, ignore them.
  } else if (!s->getFunction() && !s->getClass()) {
    seqassert(false, "only function and class definitions are allowed within classes");
  } else {
    v.push_back(s);
  }
  return v;
}

} // namespace ast
} // namespace seq
