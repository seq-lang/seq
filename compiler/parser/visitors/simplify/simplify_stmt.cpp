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
#include "parser/visitors/simplify/simplify.h"

using fmt::format;

namespace seq {
namespace ast {

StmtPtr SimplifyVisitor::transform(const Stmt *stmt) {
  if (!stmt)
    return nullptr;

  SimplifyVisitor v(ctx, preambleStmts);
  v.setSrcInfo(stmt->getSrcInfo());
  stmt->accept(v);
  if (!v.prependStmts->empty()) {
    if (v.resultStmt)
      v.prependStmts->push_back(move(v.resultStmt));
    v.resultStmt = N<SuiteStmt>(move(*v.prependStmts));
  }
  return move(v.resultStmt);
}

StmtPtr SimplifyVisitor::transform(const StmtPtr &stmt) {
  return transform(stmt.get());
}

void SimplifyVisitor::defaultVisit(const Stmt *s) { resultStmt = s->clone(); }

void SimplifyVisitor::visit(const SuiteStmt *stmt) {
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

void SimplifyVisitor::visit(const ContinueStmt *stmt) {
  if (ctx->loops.empty())
    error("continue outside of a loop");
  resultStmt = stmt->clone();
}

/// If a loop break variable is available (loop-else block), transform a break to:
///   loop_var = false; break
void SimplifyVisitor::visit(const BreakStmt *stmt) {
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

void SimplifyVisitor::visit(const ExprStmt *stmt) {
  resultStmt = N<ExprStmt>(transform(stmt->expr));
}

/// Performs assignment and unpacking transformations.
/// See parseAssignment() and unpackAssignments() for more details.
void SimplifyVisitor::visit(const AssignStmt *stmt) {
  vector<StmtPtr> stmts;
  if (stmt->rhs && stmt->rhs->getBinary() && stmt->rhs->getBinary()->inPlace) {
    /// Case 1: a += b
    seqassert(!stmt->type, "invalid AssignStmt {}", stmt->toString());
    stmts.push_back(
        parseAssignment(stmt->lhs.get(), stmt->rhs.get(), nullptr, false, true));
  } else if (stmt->type) {
    /// Case 2:
    stmts.push_back(parseAssignment(stmt->lhs.get(), stmt->rhs.get(), stmt->type.get(),
                                    true, false));
  } else {
    unpackAssignments(stmt->lhs.get(), stmt->rhs.get(), stmts, false, false);
  }
  resultStmt = stmts.size() == 1 ? move(stmts[0]) : N<SuiteStmt>(move(stmts));
}

/// Transform del a[x] to:
///   del a -> a = typeof(a)() (and removes a from the context)
///   del a[x] -> a.__delitem__(x)
void SimplifyVisitor::visit(const DelStmt *stmt) {
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

/// Transform print a to:
///   seq_print(a.__str__())
void SimplifyVisitor::visit(const PrintStmt *stmt) {
  resultStmt = N<ExprStmt>(transform(N<CallExpr>(
      N<IdExpr>(".seq_print"), N<CallExpr>(N<DotExpr>(clone(stmt->expr), "__str__")))));
}

void SimplifyVisitor::visit(const ReturnStmt *stmt) {
  if (!ctx->getLevel() || ctx->bases.back().isType())
    error("expected function body");
  resultStmt = N<ReturnStmt>(transform(stmt->expr));
}

void SimplifyVisitor::visit(const YieldStmt *stmt) {
  if (!ctx->getLevel() || ctx->bases.back().isType())
    error("expected function body");
  resultStmt = N<YieldStmt>(transform(stmt->expr));
}

/// Transform yield from a to:
///   for var in a: yield var
void SimplifyVisitor::visit(const YieldFromStmt *stmt) {
  auto var = ctx->cache->getTemporaryVar("yield");
  resultStmt = transform(
      N<ForStmt>(N<IdExpr>(var), clone(stmt->expr), N<YieldStmt>(N<IdExpr>(var))));
}

void SimplifyVisitor::visit(const AssertStmt *stmt) {
  resultStmt = N<AssertStmt>(transform(stmt->expr));
}

/// Transform while cond to:
///   while cond.__bool__()
/// Transform while cond: ... else: ... to:
///   no_break = True
///   while cond.__bool__(): ...
///   if no_break.__bool__(): ...
void SimplifyVisitor::visit(const WhileStmt *stmt) {
  ExprPtr cond = N<CallExpr>(N<DotExpr>(clone(stmt->cond), "__bool__"));
  string breakVar;
  StmtPtr assign = nullptr;
  if (stmt->elseSuite) {
    breakVar = ctx->cache->getTemporaryVar("no_break");
    assign = transform(N<AssignStmt>(N<IdExpr>(breakVar), N<BoolExpr>(true)));
  }
  ctx->loops.push_back(breakVar); // needed for transforming break in loop..else blocks
  StmtPtr whileStmt = N<WhileStmt>(transform(cond), transform(stmt->suite));
  ctx->loops.pop_back();
  if (stmt->elseSuite) {
    resultStmt = N<SuiteStmt>(
        move(assign), move(whileStmt),
        N<IfStmt>(transform(N<CallExpr>(N<DotExpr>(N<IdExpr>(breakVar), "__bool__"))),
                  transform(stmt->elseSuite)));
  } else {
    resultStmt = move(whileStmt);
  }
}

/// Transform for i in it: ... to:
///   for i in it.__iter__(): ...
/// Transform for i, j in it: ... to:
///   for tmp in it.__iter__():
///      i, j = tmp; ...
/// This transformation uses AssignStmt and supports all unpack operations that are
/// handled there.
/// Transform for i in it: ... else: ... to:
///   no_break = True
///   for i in it.__iter__(): ...
///   if no_break.__bool__(): ...
void SimplifyVisitor::visit(const ForStmt *stmt) {
  ExprPtr iter = N<CallExpr>(N<DotExpr>(clone(stmt->iter), "__iter__"));

  string breakVar;
  StmtPtr assign = nullptr;
  std::unique_ptr<ForStmt> forStmt = nullptr;
  if (stmt->elseSuite) {
    breakVar = ctx->cache->getTemporaryVar("no_break");
    assign = transform(N<AssignStmt>(N<IdExpr>(breakVar), N<BoolExpr>(true)));
  }
  ctx->loops.push_back(breakVar); // needed for transforming break in loop..else blocks
  ctx->addBlock();
  if (auto i = stmt->var->getId()) {
    string varName = i->value;
    ctx->add(SimplifyItem::Var, varName);
    forStmt = N<ForStmt>(transform(stmt->var), transform(iter), transform(stmt->suite));
  } else {
    string varName = ctx->cache->getTemporaryVar("for");
    ctx->add(SimplifyItem::Var, varName);
    auto var = N<IdExpr>(varName);
    vector<StmtPtr> stmts;
    stmts.push_back(N<AssignStmt>(clone(stmt->var), clone(var)));
    stmts.push_back(clone(stmt->suite));
    forStmt =
        N<ForStmt>(clone(var), transform(iter), transform(N<SuiteStmt>(move(stmts))));
  }

  ctx->popBlock();
  ctx->loops.pop_back();

  if (stmt->elseSuite) {
    resultStmt = N<SuiteStmt>(
        move(assign), move(forStmt),
        N<IfStmt>(transform(N<CallExpr>(N<DotExpr>(N<IdExpr>(breakVar), "__bool__"))),
                  transform(stmt->elseSuite)));
  } else {
    resultStmt = move(forStmt);
  }
}

/// Transforms all if conditions to condition.__bool__()
void SimplifyVisitor::visit(const IfStmt *stmt) {
  if (stmt->ifs.size() == 1 && !stmt->ifs[0].cond) {
    resultStmt = transform(stmt->ifs[0].suite);
    return;
  }

  vector<IfStmt::If> topIf;
  vector<IfStmt::If> subIf;

  for (auto i = 0; i < stmt->ifs.size(); ++i) {
    if (i == 0) {
      topIf.push_back(
          {transform(N<CallExpr>(N<DotExpr>(clone(stmt->ifs[i].cond), "__bool__"))),
           transform(stmt->ifs[i].suite)});
    } else {
      subIf.push_back({clone(stmt->ifs[i].cond), clone(stmt->ifs[i].suite)});
    }
  }

  if (subIf.empty()) {
    resultStmt = N<IfStmt>(move(topIf));
  } else {
    topIf.push_back({nullptr, transform(N<IfStmt>(move(subIf)))});
    resultStmt = N<IfStmt>(move(topIf));
  }
}

void SimplifyVisitor::visit(const MatchStmt *stmt) {
  auto w = transform(stmt->what);
  vector<PatternPtr> patterns;
  vector<StmtPtr> cases;
  for (auto ci = 0; ci < stmt->cases.size(); ci++) {
    ctx->addBlock();
    if (auto p = CAST(stmt->patterns[ci], BoundPattern)) {
      ctx->add(SimplifyItem::Var, p->var);
      patterns.push_back(transform(p->pattern));
      cases.push_back(transform(stmt->cases[ci]));
    } else {
      patterns.push_back(transform(stmt->patterns[ci]));
      cases.push_back(transform(stmt->cases[ci]));
    }
    ctx->popBlock();
  }
  resultStmt = N<MatchStmt>(move(w), move(patterns), move(cases));
}

void SimplifyVisitor::visit(const TryStmt *stmt) {
  vector<TryStmt::Catch> catches;
  auto suite = transform(stmt->suite);
  for (auto &ctch : stmt->catches) {
    ctx->addBlock();
    if (!ctch.var.empty())
      ctx->add(SimplifyItem::Var, ctch.var);
    catches.push_back({ctch.var, transformType(ctch.exc.get()), transform(ctch.suite)});
    ctx->popBlock();
  }
  resultStmt = N<TryStmt>(move(suite), move(catches), transform(stmt->finally));
}

void SimplifyVisitor::visit(const ThrowStmt *stmt) {
  resultStmt = N<ThrowStmt>(transform(stmt->expr));
}

/// Transform with foo(), bar() as a: ... to:
///   block:
///     tmp = foo()
///     tmp.__enter__()
///     try:
///       a = bar()
///       a.__enter__()
///       try:
///         ...
///       finally:
///         a.__exit__()
///     finally:
///       tmp.__exit__()
void SimplifyVisitor::visit(const WithStmt *stmt) {
  assert(stmt->items.size());
  vector<StmtPtr> content;
  for (int i = int(stmt->items.size()) - 1; i >= 0; i--) {
    vector<StmtPtr> internals;
    string var =
        stmt->vars[i].empty() ? ctx->cache->getTemporaryVar("with") : stmt->vars[i];
    internals.push_back(N<AssignStmt>(N<IdExpr>(var), clone(stmt->items[i])));
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

/// Perform the global checks and remove the statement from the consideration.
void SimplifyVisitor::visit(const GlobalStmt *stmt) {
  if (ctx->bases.empty() || ctx->bases.back().isType())
    error("global outside of a function");
  auto val = ctx->find(stmt->var);
  if (!val || !val->isVar())
    error("identifier '{}' not found", stmt->var);
  if (!val->getBase().empty())
    error("not a global variable");
  val->global = true;
  seqassert(!val->canonicalName.empty(), "'{}' does not have a canonical name",
            stmt->var);
  ctx->add(SimplifyItem::Var, stmt->var, val->canonicalName);
}

/// Import a module into its own context. If a module has not been imported before,
/// execute its executable statements at the current location. See below for details.
///
/// This function also handles FFI imports (C, Python etc). For the details, see
/// parseCImport(), parseCDylibImport() and parsePythonImport().
///
/// ⚠️ Warning: This behavior is slightly different than Python's
/// behavior that executes imports when they are _executed_ first.
void SimplifyVisitor::visit(const ImportStmt *stmt) {
  seqassert(!ctx->getLevel() || !ctx->bases.back().isType(), "imports within a class");
  if (stmt->from->isId("C")) {
    /// Handle C imports
    if (auto i = stmt->what->getId())
      resultStmt = parseCImport(i->value, stmt->args, stmt->ret.get(), stmt->as);
    else if (auto d = stmt->what->getDot())
      resultStmt = parseCDylibImport(d->expr.get(), d->member, stmt->args,
                                     stmt->ret.get(), stmt->as);
    else
      seqassert(false, "invalid C import statement");
    return;
  } else if (stmt->from->isId("python") && stmt->what) {
    resultStmt =
        parsePythonImport(stmt->what.get(), stmt->args, stmt->ret.get(), stmt->as);
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
  auto file = getImportFile(ctx->cache->argv0, path, ctx->getFilename());
  if (file.empty())
    error("cannot locate import '{}'", join(dirs, "."));

  auto import = ctx->cache->imports.find(file);
  // If the imported file has not been seen before, load it and cache it
  if (import == ctx->cache->imports.end()) {
    // Use a clean context for the new file.
    auto ictx = make_shared<SimplifyContext>(file, ctx->cache);
    ictx->isStdlibLoading = ctx->isStdlibLoading;
    import = ctx->cache->imports.insert({file, {file, ictx}}).first;
    StmtPtr sf = parseFile(file);
    auto sn = SimplifyVisitor(ictx, preambleStmts).transform(sf);
    if (ctx->isStdlibLoading) {
      resultStmt = N<SuiteStmt>(move(sn), true);
    } else {
      // LOG("importing {}", file);
      seqassert(preambleStmts, "preamble not set");
      import->second.importVar = ctx->cache->getTemporaryVar("import", '.');
      // imported = False
      preambleStmts->emplace_back(N<AssignStmt>(
          N<IdExpr>(import->second.importVar + "_loaded"), N<BoolExpr>(false)));
      vector<StmtPtr> stmts;
      vector<string> globalVars;
      auto processStmt = [&](StmtPtr &s) {
        if (s->getFunction() || s->getClass()) {
          preambleStmts->emplace_back(move(s));
        } else if (s->getAssign() && s->getAssign()->lhs->getId()) {
          auto a = const_cast<AssignStmt *>(s->getAssign());
          auto val = ictx->find(a->lhs->getId()->value);
          seqassert(val, "cannot locate '{}' in imported file {}",
                    s->getAssign()->lhs->getId()->value, file);
          if (val->kind == SimplifyItem::Var && val->global && val->base.empty()) {
            globalVars.emplace_back(val->canonicalName);
            preambleStmts->emplace_back(
                N<AssignStmt>(N<IdExpr>(val->canonicalName), clone(a->type)));
            stmts.push_back(N<UpdateStmt>(move(a->lhs), move(a->rhs)));
          } else {
            stmts.push_back(move(s));
          }
        } else {
          stmts.push_back(move(s));
        }
      };
      if (auto st = const_cast<SuiteStmt *>(sn->getSuite()))
        for (auto &ss : st->stmts)
          processStmt(ss);
      else
        processStmt(sn);
      // Add it to the toplevel manually and set ATTR_BUILTIN to realize them ASAP.
      ctx->cache->asts[import->second.importVar] = N<FunctionStmt>(
          import->second.importVar, nullptr, vector<Param>{}, vector<Param>{},
          N<SuiteStmt>(move(stmts)), vector<string>{ATTR_BUILTIN});
      preambleStmts->emplace_back(
          N<FunctionStmt>(import->second.importVar, nullptr, vector<Param>{},
                          vector<Param>{}, nullptr, vector<string>{ATTR_BUILTIN}));
    }
  }
  // Import variable is empty if it has already been loaded during the standard library
  // initialization.
  if (!ctx->isStdlibLoading && !import->second.importVar.empty()) {
    vector<StmtPtr> ifSuite;
    ifSuite.emplace_back(N<ExprStmt>(N<CallExpr>(N<IdExpr>(import->second.importVar))));
    ifSuite.emplace_back(N<UpdateStmt>(N<IdExpr>(import->second.importVar + "_loaded"),
                                       N<BoolExpr>(true)));
    resultStmt =
        N<IfStmt>(N<CallExpr>(N<DotExpr>(
                      N<IdExpr>(import->second.importVar + "_loaded"), "__invert__")),
                  N<SuiteStmt>(move(ifSuite)));
  }

  if (!stmt->what) {
    // Case 1: import foo
    ctx->add(SimplifyItem::Import, stmt->as.empty() ? path : stmt->as, file);
  } else if (stmt->what->isId("*")) {
    // Case 2: from foo import *
    seqassert(stmt->as.empty(), "renamed star-import");
    // Just copy all symbols from import's context here.
    for (auto &i : *(import->second.ctx))
      if (i.second.front().second->isGlobal())
        ctx->add(i.first, i.second.front().second);
  } else {
    // Case 3: from foo import bar
    auto i = stmt->what->getId();
    seqassert(i, "not a valid import what expression");
    auto c = import->second.ctx->find(i->value);
    // Make sure that we are importing an existing global symbol
    if (!c || !c->isGlobal())
      error("symbol '{}' not found in {}", i->value, file);
    ctx->add(stmt->as.empty() ? i->value : stmt->as, c);
  }
}

/// Transforms function definitions.
///
/// At this stage, the only meaningful transformation happens for "self" arguments in a
/// class method that have no type annotation (they will get one automatically).
///
/// For Python and LLVM definition transformations, see parsePythonDefinition() and
/// parseLLVMDefinition().
void SimplifyVisitor::visit(const FunctionStmt *stmt) {
  if (in(stmt->attributes, ATTR_EXTERN_PYTHON)) {
    // Handle Python code separately
    resultStmt = parsePythonDefinition(stmt->name, stmt->args, stmt->ret.get(),
                                       stmt->suite->firstInBlock());
    return;
  }

  auto canonicalName = ctx->generateCanonicalName(stmt->name);
  bool isClassMember = ctx->getLevel() && ctx->bases.back().isType();

  if (in(stmt->attributes, ATTR_BUILTIN) && (ctx->getLevel() || isClassMember))
    error("builtins must be defined at the toplevel");

  generateFunctionStub(stmt->args.size());
  if (!isClassMember)
    // Class members are added to class' method table
    ctx->add(SimplifyItem::Func, stmt->name, canonicalName, ctx->isToplevel());

  ctx->bases.emplace_back(SimplifyContext::Base{canonicalName}); // Add new base...
  ctx->addBlock();                                               // ... and a block!
  // Add generic identifiers to the context
  vector<Param> newGenerics;
  for (auto &g : stmt->generics) {
    ctx->add(SimplifyItem::Type, g.name, "", false, g.type != nullptr);
    newGenerics.emplace_back(
        Param{g.name, transformType(g.type.get()), transform(g.deflt.get(), true)});
  }
  // Parse function arguments and add them to the context.
  vector<Param> args;
  for (int ia = 0; ia < stmt->args.size(); ia++) {
    auto &a = stmt->args[ia];
    auto typeAst = transformType(a.type.get());
    // If the first argument of a class method is self and if it has no type, add it.
    if (!typeAst && isClassMember && ia == 0 && a.name == "self")
      typeAst = transformType(ctx->bases[ctx->bases.size() - 2].ast.get());
    args.emplace_back(Param{a.name, move(typeAst), transform(a.deflt)});
    ctx->add(SimplifyItem::Var, a.name);
  }
  // Parse the return type.
  if (!stmt->ret && in(stmt->attributes, ATTR_EXTERN_LLVM))
    error("LLVM functions must have a return type");
  auto ret = transformType(stmt->ret.get());
  // Parse function body.
  StmtPtr suite = nullptr;
  if (!in(stmt->attributes, ATTR_INTERNAL) && !in(stmt->attributes, ATTR_EXTERN_C)) {
    ctx->addBlock();
    if (in(stmt->attributes, ATTR_EXTERN_LLVM))
      suite = parseLLVMDefinition(stmt->suite->firstInBlock());
    else
      suite = SimplifyVisitor(ctx, preambleStmts).transform(stmt->suite);
    ctx->popBlock();
  }

  // Now fill the internal attributes that will be used later...
  auto attributes = stmt->attributes;

  // Once the body is done, check if this function refers to a variable (or generic)
  // from outer scope (e.g. it's parent is not -1). If so, store the name of the
  // innermost base that was referred to in this function.
  auto refParent =
      ctx->bases.back().parent == -1 ? "" : ctx->bases[ctx->bases.back().parent].name;
  ctx->bases.pop_back();
  ctx->popBlock();

  // Get the name of parent function (if there is any).
  // This should reach parent function even if there is a class base in the middle.
  string parentFunc;
  for (int i = int(ctx->bases.size()) - 1; i >= 0; i--)
    if (!ctx->bases[i].isType()) {
      parentFunc = ctx->bases[i].name;
      break;
    }
  attributes[ATTR_PARENT_FUNCTION] = parentFunc;

  if (isClassMember) { // If this is a method...
    // ... set the enclosing class name...
    attributes[ATTR_PARENT_CLASS] = ctx->bases.back().name;
    // ... and if the function references outer class variable (by definition a
    // generic), mark it as not static as it needs fully instantiated class to be
    // realized. For example, in class A[T]: def foo(): pass, A.foo() can be realized
    // even if T is unknown. However, def bar(): return T() cannot because it needs T
    // (and is thus accordingly marked with ATTR_NOT_STATIC).
    if ((!ctx->bases.empty() && refParent == ctx->bases.back().name) ||
        canonicalName == ".Ptr.__elemsize__" || canonicalName == ".Ptr.__atomic__")
      attributes[ATTR_NOT_STATIC] = "";
  }
  auto f = N<FunctionStmt>(canonicalName, move(ret), move(newGenerics), move(args),
                           move(suite), move(attributes));
  // Do not clone suite in the resultStmt: the suite will be accessed later trough the
  // cache.
  resultStmt =
      N<FunctionStmt>(canonicalName, clone(f->ret), clone_nop(f->generics),
                      clone_nop(f->args), nullptr, map<string, string>(f->attributes));
  // Make sure to cache this (generic) AST for later realization.
  ctx->cache->asts[canonicalName] = move(f);
}

/// Transforms type definitions and extensions.
/// This currently consists of adding default magic methods (described in
/// codegenMagic() method below).
void SimplifyVisitor::visit(const ClassStmt *stmt) {
  // Extensions (@extend) cases are handled bit differently
  // (no auto method-generation, no arguments etc.)
  bool extension = in(stmt->attributes, "extend");
  if (extension && stmt->attributes.size() != 1)
    error("extend cannot be combined with other attributes");
  if (extension && !ctx->bases.empty())
    error("extend is only allowed at the toplevel");

  bool isRecord = stmt->isRecord(); // does it have @tuple

  // Generate/find class' canonical name (unique ID) and AST
  string canonicalName;
  const ClassStmt *originalAST = nullptr;
  if (!extension) {
    canonicalName = ctx->generateCanonicalName(stmt->name);
    seqassert(ctx->bases.empty() || !ctx->bases.back().isType(),
              "nested classes not yet supported");
    // Reference types are added to the context at this stage.
    // Record types (tuples) are added after parsing class arguments to prevent
    // recursive record types (that are allowed for reference types).
    if (!isRecord)
      ctx->add(SimplifyItem::Type, stmt->name, canonicalName, ctx->isToplevel());
    originalAST = stmt;
  } else {
    // Find the canonical name of a class that is to be extended
    auto val = ctx->find(stmt->name);
    if (!val || val->kind != SimplifyItem::Type)
      error("cannot find type '{}' to extend", stmt->name);
    canonicalName = val->canonicalName;
    const auto &astIter = ctx->cache->asts.find(canonicalName);
    seqassert(astIter != ctx->cache->asts.end(), "cannot find AST for {}",
              canonicalName);
    originalAST = astIter->second->getClass();
    seqassert(originalAST, "AST for {} is not a class", canonicalName);
    if (originalAST->generics.size() != stmt->generics.size())
      error("generics do not match");
    for (int i = 0; i < originalAST->generics.size(); i++)
      if (originalAST->generics[i].name != stmt->generics[i].name)
        error("generics do not match");
  }

  // Add the class base.
  ctx->bases.emplace_back(SimplifyContext::Base(canonicalName));
  ctx->bases.back().ast = N<IdExpr>(stmt->name);

  // Add generics, if any, to the context.
  ctx->addBlock();
  vector<ExprPtr> genAst;
  vector<Param> newGenerics;
  for (const auto &g : originalAST->generics) {
    if (g.deflt)
      error("default generics not supported in classes");
    genAst.push_back(N<IdExpr>(g.name));
    ctx->add(SimplifyItem::Type, g.name, "", false, g.type != nullptr);
    newGenerics.emplace_back(Param{g.name, transformType(g.type.get()), nullptr});
  }
  ctx->bases.back().ast =
      N<IndexExpr>(N<IdExpr>(stmt->name), N<TupleExpr>(move(genAst)));

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
    }
    if (isRecord) {
      // Now that we are done with arguments, add record type to the context.
      // However, we need to unroll a block/base, add it, and add the unrolled
      // block/base back.
      ctx->popBlock();
      auto old =
          SimplifyContext::Base{ctx->bases.back().name, clone(ctx->bases.back().ast),
                                ctx->bases.back().parent};
      ctx->bases.pop_back();
      ctx->add(SimplifyItem::Type, stmt->name, canonicalName, ctx->isToplevel());
      ctx->bases.emplace_back(
          SimplifyContext::Base{old.name, move(old.ast), old.parent});
      ctx->addBlock();
      for (const auto &g : originalAST->generics)
        ctx->add(SimplifyItem::Type, g.name, "", false, g.type != nullptr);
    }

    // Create a cached AST.
    ctx->cache->asts[canonicalName] = N<ClassStmt>(
        canonicalName, move(newGenerics), move(args), N<SuiteStmt>(vector<StmtPtr>()),
        map<string, string>(stmt->attributes));

    vector<StmtPtr> fns;
    ExprPtr codeType = clone(ctx->bases.back().ast);
    vector<string> magics{};
    // Internal classes do not get any auto-generated members.
    if (!in(stmt->attributes, ATTR_INTERNAL)) {
      // Prepare a list of magics that are to be auto-generated.
      if (!isRecord) {
        magics = {"new", "init", "raw"};
        if (in(stmt->attributes, ATTR_TOTAL_ORDERING))
          for (auto &i : {"eq", "ne", "lt", "gt", "le", "ge"})
            magics.emplace_back(i);
        if (!in(stmt->attributes, ATTR_NO(ATTR_PICKLE)))
          for (auto &i : {"pickle", "unpickle"})
            magics.emplace_back(i);
      } else {
        magics = {"new", "str", "len", "hash"};
        if (!in(stmt->attributes, ATTR_NO(ATTR_TOTAL_ORDERING)))
          for (auto &i : {"eq", "ne", "lt", "gt", "le", "ge"})
            magics.emplace_back(i);
        if (!in(stmt->attributes, ATTR_NO(ATTR_PICKLE)))
          for (auto &i : {"pickle", "unpickle"})
            magics.emplace_back(i);
        if (!in(stmt->attributes, ATTR_NO(ATTR_CONTAINER)))
          for (auto &i : {"iter", "getitem", "contains"})
            magics.emplace_back(i);
        if (!in(stmt->attributes, ATTR_NO(ATTR_PYTHON)))
          for (auto &i : {"to_py", "from_py"})
            magics.emplace_back(i);
      }
    }
    // Codegen default magic methods and add them to the final AST.
    for (auto &m : magics)
      fns.push_back(codegenMagic(m, ctx->bases.back().ast.get(), stmt->args, isRecord));
    fns.push_back(clone(stmt->suite));
    for (const auto &s : fns)
      for (auto sp : getClassMethods(s.get()))
        suite->stmts.push_back(transform(sp));
  } else {
    for (auto sp : getClassMethods(stmt->suite.get()))
      suite->stmts.push_back(transform(sp));
  }
  ctx->bases.pop_back();
  ctx->popBlock();

  auto c = const_cast<ClassStmt *>(ctx->cache->asts[canonicalName]->getClass());
  if (!extension) {
    // Update the cached AST.
    seqassert(c, "not a class AST for {}", canonicalName);
    c->suite = move(suite);
    // Add parent function attribute (see visit(FunctionStmt*) for details).
    string parentFunc;
    for (int i = int(ctx->bases.size()) - 1; i >= 0; i--)
      if (!ctx->bases[i].isType()) {
        parentFunc = ctx->bases[i].name;
        break;
      }
    c->attributes[ATTR_PARENT_FUNCTION] = parentFunc;
    resultStmt = clone(ctx->cache->asts[canonicalName]);
  } else {
    resultStmt = N<ClassStmt>(canonicalName, clone_nop(c->generics), vector<Param>{},
                              move(suite), map<string, string>(stmt->attributes));
  }
}

StmtPtr SimplifyVisitor::parseAssignment(const Expr *lhs, const Expr *rhs,
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
          return N<UpdateStmt>(transform(lhs, false), transform(rhs, true));
        else if (mustExist)
          error("variable '{}' is not global", e->value);
      }
    }
    // Function and type aliases are not normal assignments. They are treated like a
    // simple context renames.
    // Note: x = Ptr[byte] is not a simple alias, and is handled separately below.
    if (rhs && rhs->getId()) {
      auto val = ctx->find(rhs->getId()->value);
      if (!val)
        error("cannot find '{}'", rhs->getId()->value);
      if (val->isType() || val->isFunc()) {
        ctx->add(e->value, val);
        return nullptr;
      }
    }
    // This assignment is a new variable assignment (not a rename or an update).
    // Generate new canonical variable name for this assignment and use it afterwards.
    // TODO: fix this
    auto canonical = ctx->isToplevel() ? ctx->generateCanonicalName(e->value) : "";
    auto l = N<IdExpr>(canonical.empty() ? e->value : canonical);
    auto r = transform(rhs, true);
    if (r && r->isType())
      ctx->add(SimplifyItem::Type, e->value, canonical, ctx->isToplevel());
    else
      /// TODO: all top-level variables are global now!
      ctx->add(SimplifyItem::Var, e->value, canonical, ctx->isToplevel());
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
    stmts.push_back(parseAssignment(lhs, rhs, nullptr, shadow, mustExist));
    return;
  }

  // Prepare the right-side expression
  auto srcPos = rhs;
  ExprPtr newRhs = nullptr; // This expression must not be deleted until the very end.
  if (!rhs->getId()) { // Store any non-trivial right-side expression (assign = rhs).
    auto var = ctx->cache->getTemporaryVar("assign");
    newRhs = Nx<IdExpr>(srcPos, var);
    stmts.push_back(parseAssignment(newRhs.get(), rhs, nullptr, shadow, mustExist));
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
  // If there is a StarExpr, process it and the remaining assignments after it (if any).
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
    // Keep going till the very end. Remaining assignments use negative indices (-1, -2
    // etc) as we are not sure how big is StarExpr.
    for (; st < leftSide.size(); st++) {
      if (leftSide[st]->getStar())
        error(leftSide[st], "multiple unpack expressions");
      rightSide = Nx<IndexExpr>(srcPos, rhs->clone(),
                                Nx<IntExpr>(srcPos, -leftSide.size() + st));
      unpackAssignments(leftSide[st], rightSide.get(), stmts, shadow, mustExist);
    }
  }
}

StmtPtr SimplifyVisitor::parseCImport(const string &name, const vector<Param> &args,
                                      const Expr *ret, const string &altName) {
  auto canonicalName = ctx->generateCanonicalName(name);
  vector<Param> fnArgs;
  generateFunctionStub(args.size());
  for (int ai = 0; ai < args.size(); ai++) {
    seqassert(args[ai].name.empty(), "unexpected argument name");
    seqassert(!args[ai].deflt, "unexpected default argument");
    seqassert(args[ai].type, "missing type");
    fnArgs.emplace_back(
        Param{args[ai].name.empty() ? format(".a{}", ai) : args[ai].name,
              transformType(args[ai].type.get()), nullptr});
  }
  ctx->add(SimplifyItem::Func, altName.empty() ? name : altName, canonicalName,
           ctx->isToplevel());
  auto f = N<FunctionStmt>(
      canonicalName, ret ? transformType(ret) : transformType(N<IdExpr>("void").get()),
      vector<Param>(), move(fnArgs), nullptr, vector<string>{ATTR_EXTERN_C});
  ctx->cache->asts[canonicalName] = clone(f);
  return f;
}

StmtPtr SimplifyVisitor::parseCDylibImport(const Expr *dylib, const string &name,
                                           const vector<Param> &args, const Expr *ret,
                                           const string &altName) {
  vector<StmtPtr> stmts;
  // fptr = _dlsym(dylib, "name")
  stmts.push_back(
      N<AssignStmt>(N<IdExpr>("fptr"), N<CallExpr>(N<IdExpr>("_dlsym"), dylib->clone(),
                                                   N<StringExpr>(name))));
  // Prepare Function[args...]
  vector<ExprPtr> fnArgs;
  fnArgs.emplace_back(ret ? ret->clone() : N<IdExpr>("void"));
  for (const auto &a : args) {
    seqassert(a.name.empty(), "unexpected argument name");
    seqassert(!a.deflt, "unexpected default argument");
    seqassert(a.type, "missing type");
    fnArgs.emplace_back(clone(a.type));
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
    fnArgs.emplace_back(N<IdExpr>(format(".a{}", i)));
  // f(args...)
  auto call = N<CallExpr>(N<IdExpr>("f"), move(fnArgs));
  if (!isVoid)
    stmts.push_back(N<ReturnStmt>(move(call)));
  else
    stmts.push_back(N<ExprStmt>(move(call)));
  vector<Param> params;
  // Prepare final FunctionStmt and transform it
  for (int i = 0; i < args.size(); i++)
    params.emplace_back(Param{format(".a{}", i), clone(args[i].type)});
  return transform(N<FunctionStmt>(
      altName.empty() ? name : altName, ret ? ret->clone() : nullptr, vector<Param>(),
      move(params), N<SuiteStmt>(move(stmts)), vector<string>()));
}

StmtPtr SimplifyVisitor::parsePythonImport(const Expr *what, const vector<Param> &args,
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
        N<CallExpr>(N<DotExpr>(N<IdExpr>("pyobj"), "_import"), N<StringExpr>(name))));

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
      move(params), N<SuiteStmt>(move(call), move(retStmt)), vector<string>()));
}

StmtPtr SimplifyVisitor::parsePythonDefinition(const string &name,
                                               const vector<Param> &args,
                                               const Expr *ret, const Stmt *codeStmt) {
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
                    clone_nop(args), ret ? ret->clone() : nullptr)));
}

StmtPtr SimplifyVisitor::parseLLVMDefinition(const Stmt *codeStmt) {
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
      auto expr = transformIndexExpr(parseExpr(exprCode, offset));
      if (!expr->isType() && !expr->getStatic())
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

string SimplifyVisitor::generateFunctionStub(int n) {
  seqassert(n >= 0, "invalid n");
  auto typeName = format("Function.{}", n);
  if (ctx->cache->variardics.find(typeName) == ctx->cache->variardics.end()) {
    ctx->cache->variardics.insert(typeName);

    vector<Param> generics;
    generics.emplace_back(Param{"TR", nullptr, nullptr});
    vector<ExprPtr> genericNames;
    genericNames.emplace_back(N<IdExpr>("TR"));
    // TODO: remove this args hack
    vector<Param> args;
    args.emplace_back(Param{".ret", N<IdExpr>("TR"), nullptr});
    for (int i = 1; i <= n; i++) {
      genericNames.emplace_back(N<IdExpr>(format("T{}", i)));
      generics.emplace_back(Param{format("T{}", i), nullptr, nullptr});
      args.emplace_back(Param{format(".a{}", i), N<IdExpr>(format("T{}", i)), nullptr});
    }
    ExprPtr type = N<IndexExpr>(N<IdExpr>(typeName), N<TupleExpr>(move(genericNames)));

    vector<StmtPtr> fns;
    vector<Param> params;
    // def __new__(what: Ptr[byte]) -> Function.N[TR, T1, ..., TN]: pass
    params.emplace_back(
        Param{"what", N<IndexExpr>(N<IdExpr>("Ptr"), N<IdExpr>("byte"))});
    fns.emplace_back(make_unique<FunctionStmt>("__new__", clone(type), vector<Param>{},
                                               move(params), nullptr,
                                               vector<string>{ATTR_INTERNAL}));
    params.clear();
    // def __str__(self: Function.N[TR, T1, ..., TN]) -> str: pass
    params.emplace_back(Param{"self", clone(type)});
    fns.emplace_back(make_unique<FunctionStmt>("__str__", N<IdExpr>("str"),
                                               vector<Param>{}, move(params), nullptr,
                                               vector<string>{ATTR_INTERNAL}));
    // class Function.N[TR, T1, ..., TN]
    StmtPtr stmt = make_unique<ClassStmt>(
        typeName, move(generics), move(args), N<SuiteStmt>(move(fns)),
        vector<string>{ATTR_INTERNAL, ATTR_TRAIT, ATTR_TUPLE});
    stmt->setSrcInfo(ctx->generateSrcInfo());
    // Parse this function in a clean context.
    SimplifyVisitor(make_shared<SimplifyContext>("<generated>", ctx->cache), nullptr)
        .transform(stmt);
  }
  return "." + typeName;
}

StmtPtr SimplifyVisitor::codegenMagic(const string &op, const Expr *typExpr,
                                      const vector<Param> &args, bool isRecord) {
#define I(s) N<IdExpr>(s)
  assert(typExpr);
  ExprPtr ret;
  vector<Param> fargs;
  vector<StmtPtr> stmts;
  vector<string> attrs;
  if (op == "new") {
    // Classes: @internal def __new__() -> T
    // Tuples: @internal def __new__(a1: T1, ..., aN: TN) -> T
    ret = typExpr->clone();
    if (isRecord)
      for (auto &a : args)
        fargs.emplace_back(
            Param{a.name, clone(a.type),
                  a.deflt ? clone(a.deflt) : N<CallExpr>(clone(a.type))});
    attrs.emplace_back(ATTR_INTERNAL);
  } else if (op == "init") {
    // Classes: def __init__(self: T, a1: T1, ..., aN: TN) -> void
    ret = I("void");
    fargs.emplace_back(Param{"self", typExpr->clone()});
    for (auto &a : args) {
      stmts.push_back(N<AssignMemberStmt>(I("self"), a.name, I(a.name)));
      fargs.emplace_back(Param{a.name, clone(a.type),
                               a.deflt ? clone(a.deflt) : N<CallExpr>(clone(a.type))});
    }
  } else if (op == "raw") {
    // Classes: @internal def __raw__(self: T) -> Ptr[byte]
    fargs.emplace_back(Param{"self", typExpr->clone()});
    ret = N<IndexExpr>(N<IdExpr>("Ptr"), N<IdExpr>("byte"));
    attrs.emplace_back(ATTR_INTERNAL);
  } else if (op == "getitem") {
    // Tuples: @internal def __getitem__(self: T, index: int) -> T
    //         (not a real internal; code generated during a realizeFunc() method)
    fargs.emplace_back(Param{"self", typExpr->clone()});
    fargs.emplace_back(Param{"index", I("int")});
    ret = !args.empty() ? clone(args[0].type) : I("void");
    attrs.emplace_back(ATTR_INTERNAL);
  } else if (op == "iter") {
    // Tuples: @internal def __iter__(self: T) -> Generator[T]
    //         (not a real internal; code generated during a realizeFunc() method)
    fargs.emplace_back(Param{"self", typExpr->clone()});
    ret = N<IndexExpr>(I("Generator"), !args.empty() ? clone(args[0].type) : I("void"));
    for (auto &a : args)
      stmts.emplace_back(N<YieldStmt>(N<DotExpr>(N<IdExpr>("self"), a.name)));
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
      stmts.push_back(N<UpdateStmt>(
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
  } else if (op == "contains") {
    // Tuples: @internal def __contains__(self: T, what: T1) -> bool
    //         (not a real internal; code generated during a realizeFunc() method)
    fargs.emplace_back(Param{"self", typExpr->clone()});
    fargs.emplace_back(Param{"what", !args.empty() ? clone(args[0].type) : I("void")});
    ret = I("bool");
    attrs.emplace_back(ATTR_INTERNAL);
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
    //   a.__setitem__(0, self.arg1.__str__()) ...
    //   return _tuple_str(a.ptr, N)
    fargs.emplace_back(Param{"self", typExpr->clone()});
    ret = I("str");
    if (!args.empty()) {
      stmts.emplace_back(
          N<AssignStmt>(I("a"), N<CallExpr>(N<IndexExpr>(I("__array__"), I("str")),
                                            N<IntExpr>(args.size()))));
      for (int i = 0; i < args.size(); i++)
        stmts.push_back(N<ExprStmt>(N<CallExpr>(
            N<DotExpr>(I("a"), "__setitem__"), N<IntExpr>(i),
            N<CallExpr>(N<DotExpr>(N<DotExpr>(I("self"), args[i].name), "__str__")))));
      stmts.emplace_back(N<ReturnStmt>(N<CallExpr>(N<DotExpr>(I("str"), "_tuple_str"),
                                                   N<DotExpr>(I("a"), "ptr"),
                                                   N<IntExpr>(args.size()))));
    } else {
      stmts.emplace_back(N<ReturnStmt>(N<StringExpr>("()")));
    }
  } else {
    seqassert(false, "invalid magic {}", op);
  }
#undef I
  auto t =
      make_unique<FunctionStmt>(format("__{}__", op), move(ret), vector<Param>{},
                                move(fargs), N<SuiteStmt>(move(stmts)), move(attrs));
  t->setSrcInfo(ctx->generateSrcInfo());
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
  } else if (!s->getFunction()) {
    seqassert(false, "only functions definitions are allowed within classes");
  } else {
    v.push_back(s);
  }
  return v;
}

} // namespace ast
} // namespace seq
