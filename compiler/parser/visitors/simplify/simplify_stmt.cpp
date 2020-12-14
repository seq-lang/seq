/*
 * simplify_statement.cpp --- AST statement simplifications.
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
#include "parser/common.h"
#include "parser/ocaml/ocaml.h"
#include "parser/visitors/simplify/simplify.h"

using fmt::format;

namespace seq {
namespace ast {

StmtPtr SimplifyVisitor::transform(const StmtPtr &stmt) {
  if (!stmt)
    return nullptr;

  SimplifyVisitor v(ctx);
  v.setSrcInfo(stmt->getSrcInfo());
  stmt->accept(v);
  if (!v.prependStmts->empty()) {
    if (v.resultStmt)
      v.prependStmts->push_back(move(v.resultStmt));
    v.resultStmt = N<SuiteStmt>(move(*v.prependStmts));
  }
  return move(v.resultStmt);
}

void SimplifyVisitor::defaultVisit(const Stmt *s) { resultStmt = s->clone(); }

void SimplifyVisitor::visit(const SuiteStmt *stmt) {
  vector<StmtPtr> r;
  // Make sure to add context blocks if this suite requires it...
  if (stmt->ownBlock)
    ctx->addBlock();
  for (auto &s : stmt->stmts)
    if (auto t = transform(s))
      r.push_back(move(t));
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

void SimplifyVisitor::visit(const AssignStmt *stmt) {
  auto add = [&](const ExprPtr &lhs, const ExprPtr &rhs, const ExprPtr &type,
                 bool shadow, bool mustExist) -> StmtPtr {
    auto p = lhs.get();
    if (auto l = CAST(lhs, IndexExpr)) {
      vector<ExprPtr> args;
      args.push_back(clone(l->index));
      args.push_back(clone(rhs));
      return transform(
          Nx<ExprStmt>(p, Nx<CallExpr>(p, Nx<DotExpr>(p, clone(l->expr), "__setitem__"),
                                       move(args))));
    } else if (auto l = CAST(lhs, DotExpr)) {
      return Nx<AssignMemberStmt>(p, transform(l->expr), l->member, transform(rhs));
    } else if (auto l = CAST(lhs, IdExpr)) {
      auto s = Nx<AssignStmt>(p, clone(lhs), transform(rhs, true), transformType(type));
      if (!shadow && !s->type) {
        auto val = ctx->find(l->value);
        if (val && val->isVar()) {
          if (val->getBase() == ctx->getBase())
            return Nx<UpdateStmt>(p, transform(lhs), move(s->rhs));
          else if (mustExist)
            error("variable '{}' is not global", l->value);
        }
      }
      if (auto r = CAST(rhs, IdExpr)) { // simple rename?
        auto val = ctx->find(r->value);
        if (!val)
          error("cannot find '{}'", r->value);
        if (val->isType() || val->isFunc()) {
          ctx->add(l->value, val);
          return nullptr;
        }
      }
      auto canonical = ctx->isToplevel() ? ctx->generateCanonicalName(l->value) : "";
      if (!canonical.empty())
        s->lhs = Nx<IdExpr>(p, canonical);
      if (s->rhs && s->rhs->isType())
        ctx->add(SimplifyItem::Type, l->value, canonical, ctx->isToplevel());
      else
        /// TODO: all toplevel variables are global now!
        ctx->add(SimplifyItem::Var, l->value, canonical, ctx->isToplevel());
      return s;
    } else {
      error("invalid assignment");
      return nullptr;
    }
  };
  std::function<void(const ExprPtr &, const ExprPtr &, vector<StmtPtr> &, bool, bool)>
      process = [&](const ExprPtr &lhs, const ExprPtr &rhs, vector<StmtPtr> &stmts,
                    bool shadow, bool mustExist) -> void {
    vector<ExprPtr> lefts;
    if (auto l = CAST(lhs, TupleExpr)) {
      for (auto &i : l->items)
        lefts.push_back(clone(i));
    } else if (auto l = CAST(lhs, ListExpr)) {
      for (auto &i : l->items)
        lefts.push_back(clone(i));
    } else {
      stmts.push_back(add(lhs, rhs, nullptr, shadow, mustExist));
      return;
    }
    auto p = rhs.get();
    ExprPtr newRhs = nullptr;
    if (!CAST(rhs, IdExpr)) { // store any non-trivial expression
      auto var = ctx->cache->getTemporaryVar("assign");
      newRhs = Nx<IdExpr>(p, var);
      stmts.push_back(add(newRhs, rhs, nullptr, shadow, mustExist));
    } else {
      newRhs = clone(rhs);
    }
    StarExpr *unpack = nullptr;
    int st = 0;
    for (; st < lefts.size(); st++) {
      if (auto u = CAST(lefts[st], StarExpr)) {
        unpack = u;
        break;
      }
      process(lefts[st], Nx<IndexExpr>(p, clone(newRhs), Nx<IntExpr>(p, st)), stmts,
              shadow, mustExist);
    }
    if (unpack) {
      process(unpack->what,
              Nx<IndexExpr>(p, clone(newRhs),
                            Nx<SliceExpr>(p, Nx<IntExpr>(p, st),
                                          lefts.size() == st + 1
                                              ? nullptr
                                              : Nx<IntExpr>(p, -lefts.size() + st + 1),
                                          nullptr)),
              stmts, shadow, mustExist);
      st += 1;
      for (; st < lefts.size(); st++) {
        if (CAST(lefts[st], StarExpr))
          error(lefts[st], "multiple unpack expressions found");
        process(lefts[st],
                Nx<IndexExpr>(p, clone(newRhs), Nx<IntExpr>(p, -lefts.size() + st)),
                stmts, shadow, mustExist);
      }
    }
  };

  vector<StmtPtr> stmts;
  if (stmt->rhs && stmt->rhs->getBinary() && stmt->rhs->getBinary()->inPlace) {
    seqassert(!stmt->type, "invalid AssignStmt {}", stmt->toString());
    process(stmt->lhs, stmt->rhs, stmts, false, true);
  } else if (stmt->type) {
    if (stmt->lhs->getId())
      stmts.push_back(add(stmt->lhs, stmt->rhs, stmt->type, true, false));
    else
      error("invalid type specifier");
  } else {
    process(stmt->lhs, stmt->rhs, stmts, false, false);
  }
  resultStmt = stmts.size() == 1 ? move(stmts[0]) : N<SuiteStmt>(move(stmts));
}

void SimplifyVisitor::visit(const DelStmt *stmt) {
  if (auto expr = CAST(stmt->expr, IndexExpr)) {
    resultStmt = N<ExprStmt>(transform(
        N<CallExpr>(N<DotExpr>(clone(expr->expr), "__delitem__"), clone(expr->index))));
  } else if (auto expr = CAST(stmt->expr, IdExpr)) {
    ctx->remove(expr->value);
  } else {
    error("expression cannot be deleted");
  }
}

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

void SimplifyVisitor::visit(const YieldFromStmt *stmt) {
  auto var = ctx->cache->getTemporaryVar("yield");
  resultStmt = transform(
      N<ForStmt>(N<IdExpr>(var), clone(stmt->expr), N<YieldStmt>(N<IdExpr>(var))));
}

void SimplifyVisitor::visit(const AssertStmt *stmt) {
  resultStmt = N<AssertStmt>(transform(stmt->expr));
}

void SimplifyVisitor::visit(const WhileStmt *stmt) {
  ExprPtr cond = N<CallExpr>(N<DotExpr>(clone(stmt->cond), "__bool__"));
  string breakVar;
  StmtPtr assign = nullptr;
  if (stmt->elseSuite) {
    breakVar = ctx->cache->getTemporaryVar("no_break");
    assign = N<AssignStmt>(N<IdExpr>(breakVar), N<BoolExpr>(true));
  }
  ctx->loops.push_back(breakVar);
  StmtPtr whilestmt = N<WhileStmt>(transform(cond), transform(stmt->suite));
  ctx->loops.pop_back();
  if (stmt->elseSuite) {
    resultStmt =
        N<SuiteStmt>(move(assign), move(whilestmt),
                     N<IfStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(breakVar), "__bool__")),
                               transform(stmt->elseSuite)));
  } else {
    resultStmt = move(whilestmt);
  }
}

void SimplifyVisitor::visit(const ForStmt *stmt) {
  ExprPtr iter = N<CallExpr>(N<DotExpr>(clone(stmt->iter), "__iter__"));
  string breakVar;
  StmtPtr assign = nullptr, forstmt = nullptr;
  if (stmt->elseSuite) {
    breakVar = ctx->cache->getTemporaryVar("no_break");
    assign = N<AssignStmt>(N<IdExpr>(breakVar), N<BoolExpr>(true));
  }
  ctx->loops.push_back(breakVar);
  ctx->addBlock();
  if (auto i = CAST(stmt->var, IdExpr)) {
    string varName = i->value;
    ctx->add(SimplifyItem::Var, varName);
    forstmt = N<ForStmt>(transform(stmt->var), transform(iter), transform(stmt->suite));
  } else {
    string varName = ctx->cache->getTemporaryVar("for");
    ctx->add(SimplifyItem::Var, varName);
    auto var = N<IdExpr>(varName);
    vector<StmtPtr> stmts;
    stmts.push_back(N<AssignStmt>(clone(stmt->var), clone(var)));
    stmts.push_back(clone(stmt->suite));
    forstmt =
        N<ForStmt>(clone(var), transform(iter), transform(N<SuiteStmt>(move(stmts))));
  }
  ctx->popBlock();
  ctx->loops.pop_back();

  if (stmt->elseSuite) {
    resultStmt =
        N<SuiteStmt>(move(assign), move(forstmt),
                     N<IfStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(breakVar), "__bool__")),
                               transform(stmt->elseSuite)));
  } else {
    resultStmt = move(forstmt);
  }
}

void SimplifyVisitor::visit(const IfStmt *stmt) {
  vector<IfStmt::If> ifs;
  for (auto &i : stmt->ifs)
    ifs.push_back({transform(i.cond ? N<CallExpr>(N<DotExpr>(clone(i.cond), "__bool__"))
                                    : nullptr),
                   transform(i.suite)});
  resultStmt = N<IfStmt>(move(ifs));
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
  for (auto &c : stmt->catches) {
    ctx->addBlock();
    if (c.var != "")
      ctx->add(SimplifyItem::Var, c.var);
    catches.push_back({c.var, transformType(c.exc), transform(c.suite)});
    ctx->popBlock();
  }
  resultStmt = N<TryStmt>(move(suite), move(catches), transform(stmt->finally));
}

void SimplifyVisitor::visit(const ThrowStmt *stmt) {
  resultStmt = N<ThrowStmt>(transform(stmt->expr));
}

void SimplifyVisitor::visit(const WithStmt *stmt) {
  assert(stmt->items.size());
  vector<StmtPtr> content;
  for (int i = stmt->items.size() - 1; i >= 0; i--) {
    vector<StmtPtr> internals;
    string var =
        stmt->vars[i] == "" ? ctx->cache->getTemporaryVar("with") : stmt->vars[i];
    internals.push_back(N<AssignStmt>(N<IdExpr>(var), clone(stmt->items[i])));
    internals.push_back(
        N<ExprStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "__enter__"))));

    internals.push_back(N<TryStmt>(
        content.size() ? N<SuiteStmt>(move(content), true) : clone(stmt->suite),
        vector<TryStmt::Catch>{},
        N<SuiteStmt>(N<ExprStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "__exit__"))),
                     true)));
    content = move(internals);
  }
  resultStmt = transform(N<SuiteStmt>(move(content), true));
}

void SimplifyVisitor::visit(const GlobalStmt *stmt) {
  if (!ctx->bases.size() || ctx->bases.back().isType())
    error("'global' is only applicable within function blocks");
  auto val = ctx->find(stmt->var);
  if (!val || !val->isVar())
    error("identifier '{}' not found", stmt->var);
  if (val->getBase() != "")
    error("not a toplevel variable");
  val->global = true;
  seqassert(!val->canonicalName.empty(), "'{}' does not have a canonical name",
            stmt->var);
  ctx->add(SimplifyItem::Var, stmt->var, val->canonicalName);
}

void SimplifyVisitor::visit(const ImportStmt *stmt) {
  if (ctx->getLevel() && ctx->bases.back().isType())
    error("imports cannot be located within classes");

  if (stmt->from->isId("C")) {
    if (auto i = stmt->what->getId())
      resultStmt = parseCImport(i->value, stmt->args, stmt->ret, stmt->as);
    else if (auto d = stmt->what->getDot())
      resultStmt =
          parseDylibCImport(d->expr, d->member, stmt->args, stmt->ret, stmt->as);
    else
      error("invalid C import");
    return;
  } else if (stmt->from->isId("python")) {
    resultStmt = parsePythonImport(stmt->what, stmt->as);
    return;
  }

  vector<string> dirs;
  Expr *e = stmt->from.get();
  while (auto d = dynamic_cast<DotExpr *>(e)) {
    dirs.push_back(d->member);
    e = d->expr.get();
  }
  if (!e->getId() || !stmt->args.empty() || stmt->ret ||
      (stmt->what && !stmt->what->getId()))
    error("invalid import statement");
  dirs.push_back(e->getId()->value);
  // TODO: enforce locality with ".abc"
  seqassert(stmt->dots >= 0, "invalid ImportStmt.dots");
  for (int i = 0; i < stmt->dots - 1; i++)
    dirs.push_back("..");
  string path;
  for (int i = dirs.size() - 1; i >= 0; i--)
    path += dirs[i] + (i ? "/" : "");
  auto file = getImportFile(ctx->cache->argv0, path, ctx->getFilename());
  if (file.empty())
    error("cannot locate import '{}'", stmt->from->toString());

  auto import = ctx->cache->imports.find(file);
  if (import == ctx->cache->imports.end()) {
    auto ictx = make_shared<SimplifyContext>(file, ctx->cache);
    import = ctx->cache->imports.insert({file, {file, ictx}}).first;
    StmtPtr s = parseFile(file);
    auto sn = SimplifyVisitor(ictx).transform(s);
    resultStmt = N<SuiteStmt>(move(sn), true);
  }

  if (!stmt->what) {
    ctx->add(SimplifyItem::Import, stmt->as.empty() ? path : stmt->as, file);
  } else if (stmt->what->isId("*")) {
    if (!stmt->as.empty())
      error("cannot rename star-import");
    for (auto &i : *(import->second.ctx))
      if (i.second.front().second->isGlobal())
        ctx->add(i.first, i.second.front().second);
  } else {
    auto i = stmt->what->getId();
    seqassert(i, "not a valid expression");
    auto c = import->second.ctx->find(i->value);
    if (!c || !c->isGlobal())
      error("symbol '{}' not found in {}", i->value, file);
    ctx->add(stmt->as.empty() ? i->value : stmt->as, c);
  }
}

void SimplifyVisitor::visit(const FunctionStmt *stmt) {
  if (in(stmt->attributes, "python")) {
    auto s = CAST(stmt->suite, ExprStmt);
    if (!s) {
      auto ss = CAST(stmt->suite, SuiteStmt);
      if (ss && ss->stmts.size() == 1)
        s = CAST(ss->stmts[0], ExprStmt);
    }
    if (!s || !CAST(s->expr, StringExpr))
      error("malformed external definition");
    string code = CAST(s->expr, StringExpr)->value;
    vector<string> args;
    for (auto &a : stmt->args)
      args.push_back(a.name);
    code = format("def {}({}):\n{}\n", stmt->name, fmt::join(args, ", "), code);
    resultStmt = transform(N<SuiteStmt>(
        N<ExprStmt>(N<CallExpr>(N<IdExpr>("_py_exec"), N<StringExpr>(code))),
        N<ImportStmt>(N<IdExpr>("python"), N<IdExpr>(stmt->name), clone_nop(stmt->args),
                      clone(stmt->ret))));
    return;
  }

  auto canonicalName = ctx->generateCanonicalName(stmt->name);
  bool isClassMember = ctx->getLevel() && ctx->bases.back().isType();

  // if (in(stmt->attributes, "llvm")) {
  //   auto s = CAST(stmt->suite, SuiteStmt);
  //   assert(s && s->stmts.size() == 1);
  //   auto sp = CAST(s->stmts[0], ExprStmt);
  //   seqassert(sp && CAST(sp->expr, StringExpr), "invalid llvm");

  //   vector<Param> args;
  //   for (int ia = 0; ia < stmt->args.size(); ia++) {
  //     auto &a = stmt->args[ia];
  //     auto typeAst = transformType(a.type);
  //     if (!typeAst && isClassMember && ia == 0 && a.name == "self")
  //       typeAst = transformType(ctx->bases[ctx->bases.size() - 1].tmp);
  //     args.push_back(Param{a.name, move(typeAst), simplify(a.deflt)});
  //   }
  //   resultStmt =
  //       parseCImport(stmt->name, args, stmt->ret, "", CAST(sp->expr, StringExpr));
  //   return;
  // }

  if (in(stmt->attributes, "builtin") && (ctx->getLevel() || isClassMember))
    error("builtins must be defined at the toplevel");

  generateFunctionStub(stmt->args.size() + 1);
  if (!isClassMember)
    ctx->add(SimplifyItem::Func, stmt->name, canonicalName, ctx->isToplevel());

  ctx->bases.push_back(SimplifyContext::Base{canonicalName});
  ctx->addBlock();
  vector<Param> newGenerics;
  for (auto &g : stmt->generics) {
    ctx->add(SimplifyItem::Type, g.name, "", false, g.type != nullptr);
    newGenerics.push_back(
        Param{g.name, transformType(g.type), transform(g.deflt, true)});
  }

  vector<Param> args;
  for (int ia = 0; ia < stmt->args.size(); ia++) {
    auto &a = stmt->args[ia];
    auto typeAst = transformType(a.type);
    if (!typeAst && isClassMember && ia == 0 && a.name == "self")
      typeAst = transformType(ctx->bases[ctx->bases.size() - 2].ast);
    args.push_back(Param{a.name, move(typeAst), transform(a.deflt)});
    ctx->add(SimplifyItem::Var, a.name);
  }
  if (!stmt->ret && in(stmt->attributes, "llvm"))
    error("LLVM functions must have a return type");
  auto ret = transformType(stmt->ret);
  StmtPtr suite = nullptr;
  if (!in(stmt->attributes, ATTR_INTERNAL) && !in(stmt->attributes, ".c")) {
    ctx->addBlock();
    if (in(stmt->attributes, "llvm"))
      suite = parseLLVMImport(stmt->suite->firstInBlock());
    else
      suite = SimplifyVisitor(ctx).transform(stmt->suite);
    ctx->popBlock();
  }

  auto refParent =
      ctx->bases.back().parent == -1 ? "" : ctx->bases[ctx->bases.back().parent].name;
  ctx->bases.pop_back();
  ctx->popBlock();

  string parentFunc = "";
  for (int i = int(ctx->bases.size()) - 1; i >= 0; i--)
    if (!ctx->bases[i].isType()) {
      parentFunc = ctx->bases[i].name;
      break;
    }
  bool isMethod = (ctx->bases.size() && refParent == ctx->bases.back().name);
  if (canonicalName == ".Ptr.__elemsize__" || canonicalName == ".Ptr.__atomic__")
    isMethod = true;

  auto attributes = stmt->attributes;
  // parentFunc: outer function scope (not class)
  // class: outer class scope
  // method: set if function is a method; usually set iff it references
  attributes[".parentFunc"] = parentFunc;
  if (isClassMember) {
    attributes[".class"] = ctx->bases.back().name;
    if (isMethod)
      attributes[".method"] = "";
  }
  resultStmt = N<FunctionStmt>(canonicalName, move(ret), move(newGenerics), move(args),
                               move(suite), move(attributes));
  ctx->cache->asts[canonicalName] = clone(resultStmt);
}

void SimplifyVisitor::visit(const ClassStmt *stmt) {
  bool extension = in(stmt->attributes, "extend");
  if (extension && stmt->attributes.size() != 1)
    error("extend cannot be mixed with other attributes");
  if (extension && ctx->bases.size())
    error("extend only valid at the toplevel");

  bool isRecord = stmt->isRecord();

  string canonicalName;
  const ClassStmt *originalAST = nullptr;
  if (!extension) {
    canonicalName = ctx->generateCanonicalName(stmt->name);
    if (ctx->bases.size() && ctx->bases.back().isType())
      error("nested classes are not supported");
    if (!isRecord)
      ctx->add(SimplifyItem::Type, stmt->name, canonicalName, ctx->isToplevel());
    originalAST = stmt;
  } else {
    auto val = ctx->find(stmt->name);
    if (!val && val->kind != SimplifyItem::Type)
      error("cannot find type {} to extend", stmt->name);
    canonicalName = val->canonicalName;
    const auto &astIter = ctx->cache->asts.find(canonicalName);
    assert(astIter != ctx->cache->asts.end());
    originalAST = CAST(astIter->second, ClassStmt);
    assert(originalAST);
    if (originalAST->generics.size() != stmt->generics.size())
      error("generics do not match");
  }

  ctx->bases.push_back(SimplifyContext::Base(canonicalName));
  ctx->bases.back().ast = N<IdExpr>(stmt->name);
  if (stmt->generics.size()) {
    vector<ExprPtr> genAst;
    for (int gi = 0; gi < originalAST->generics.size(); gi++)
      genAst.push_back(N<IdExpr>(stmt->generics[gi].name));
    ctx->bases.back().ast =
        N<IndexExpr>(N<IdExpr>(stmt->name), N<TupleExpr>(move(genAst)));
  }

  ctx->addBlock();
  vector<Param> newGenerics;
  for (int gi = 0; gi < originalAST->generics.size(); gi++) {
    if (originalAST->generics[gi].deflt)
      error("default generics not supported in types");
    ctx->add(SimplifyItem::Type, stmt->generics[gi].name, "", false,
             originalAST->generics[gi].type != nullptr);
    newGenerics.push_back(Param{stmt->generics[gi].name,
                                transformType(originalAST->generics[gi].type),
                                transform(originalAST->generics[gi].deflt, true)});
  }
  vector<Param> args;
  auto suite = N<SuiteStmt>(vector<StmtPtr>{});
  if (!extension) {
    unordered_set<string> seenMembers;
    for (auto &a : stmt->args) {
      seqassert(a.type, "no type provided for '{}'", a.name);
      if (seenMembers.find(a.name) != seenMembers.end())
        error(a.type, "'{}' declared twice", a.name);
      seenMembers.insert(a.name);
      args.push_back(Param{a.name, transformType(a.type), nullptr});
    }
    if (isRecord) {
      ctx->popBlock();

      auto old =
          SimplifyContext::Base{ctx->bases.back().name, clone(ctx->bases.back().ast),
                                ctx->bases.back().parent};
      ctx->bases.pop_back();
      ctx->add(SimplifyItem::Type, stmt->name, canonicalName, ctx->isToplevel());
      ctx->bases.push_back(SimplifyContext::Base{old.name, move(old.ast), old.parent});
      ctx->addBlock();
      for (int gi = 0; gi < originalAST->generics.size(); gi++)
        ctx->add(SimplifyItem::Type, stmt->generics[gi].name, "", false,
                 originalAST->generics[gi].type != nullptr);
    }

    ctx->cache->asts[canonicalName] = N<ClassStmt>(
        canonicalName, move(newGenerics), move(args), N<SuiteStmt>(vector<StmtPtr>()),
        map<string, string>(stmt->attributes));

    vector<StmtPtr> fns;
    ExprPtr codeType = clone(ctx->bases.back().ast);
    vector<string> magics{};
    if (!in(stmt->attributes, ATTR_INTERNAL)) {
      if (!isRecord) {
        magics = {"new", "init", "raw"};
        if (in(stmt->attributes, "total_ordering"))
          for (auto &i : {"eq", "ne", "lt", "gt", "le", "ge"})
            magics.push_back(i);
        if (!in(stmt->attributes, "no_pickle"))
          for (auto &i : {"pickle", "unpickle"})
            magics.push_back(i);
      } else {
        magics = {"new", "str", "len", "hash"};
        if (!in(stmt->attributes, "no_total_ordering"))
          for (auto &i : {"eq", "ne", "lt", "gt", "le", "ge"})
            magics.push_back(i);
        if (!in(stmt->attributes, "no_pickle"))
          for (auto &i : {"pickle", "unpickle"})
            magics.push_back(i);
        if (!in(stmt->attributes, "no_container"))
          for (auto &i : {"iter", "getitem", "contains"})
            magics.push_back(i);
        if (!in(stmt->attributes, "no_python"))
          for (auto &i : {"to_py", "from_py"})
            magics.push_back(i);
      }
    }
    for (auto &m : magics)
      fns.push_back(codegenMagic(m, ctx->bases.back().ast, stmt->args, isRecord));
    fns.push_back(clone(stmt->suite));
    for (auto &s : fns)
      for (auto &sp : addMethods(s))
        suite->stmts.push_back(move(sp));
  } else {
    for (auto &sp : addMethods(stmt->suite))
      suite->stmts.push_back(move(sp));
  }
  ctx->bases.pop_back();
  ctx->popBlock();

  if (!extension) {
    auto c = static_cast<ClassStmt *>(ctx->cache->asts[canonicalName].get());
    c->suite = move(suite);
    string parentFunc = "";
    for (int i = int(ctx->bases.size()) - 1; i >= 0; i--)
      if (!ctx->bases[i].isType()) {
        parentFunc = ctx->bases[i].name;
        break;
      }
    c->attributes[".parentFunc"] = parentFunc;
    resultStmt = clone(ctx->cache->asts[canonicalName]);
  } else {
    resultStmt = N<ClassStmt>(canonicalName, move(newGenerics), move(args), move(suite),
                              map<string, string>(stmt->attributes));
  }
}

string SimplifyVisitor::generateFunctionStub(int len) {
  assert(len >= 1);
  auto typeName = fmt::format("Function.{}", len - 1);
  if (ctx->cache->variardics.find(typeName) == ctx->cache->variardics.end()) {
    ctx->cache->variardics.insert(typeName);

    vector<Param> generics, args;
    vector<ExprPtr> genericNames;
    for (int i = 1; i <= len; i++) {
      genericNames.push_back(N<IdExpr>(format("T{}", i)));
      generics.push_back(Param{format("T{}", i), nullptr, nullptr});
      args.push_back(Param{format(".a{0}", i), N<IdExpr>(format("T{}", i)), nullptr});
    }
    ExprPtr type = N<IdExpr>(typeName);
    if (genericNames.size())
      type = N<IndexExpr>(move(type), N<TupleExpr>(move(genericNames)));

    vector<StmtPtr> fns;
    vector<Param> p;
    p.push_back(Param{"what", N<IndexExpr>(N<IdExpr>("Ptr"), N<IdExpr>("byte"))});
    fns.push_back(make_unique<FunctionStmt>("__new__", clone(type), vector<Param>{},
                                            move(p), nullptr,
                                            vector<string>{ATTR_INTERNAL}));
    p.clear();
    p.push_back(Param{"self", clone(type)});
    fns.push_back(make_unique<FunctionStmt>("__str__", N<IdExpr>("str"),
                                            vector<Param>{}, move(p), nullptr,
                                            vector<string>{ATTR_INTERNAL}));
    // p.clear();
    // p.push_back({"self", clone(type)});
    // for (int i = 2; i <= len; i++)
    //   p.push_back({format(".a{0}", i), N<IdExpr>(format("T{}", i)), nullptr});
    // fns.push_back(make_unique<FunctionStmt>("__call__", N<IdExpr>("T1"),
    //                                         vector<Param>{}, move(p), nullptr,
    //                                         vector<string>{ATTR_INTERNAL}));

    StmtPtr stmt = make_unique<ClassStmt>(
        typeName, move(generics), clone_nop(args), N<SuiteStmt>(move(fns)),
        vector<string>{ATTR_INTERNAL, "trait", ATTR_TUPLE});
    stmt->setSrcInfo(ctx->generateSrcInfo());
    SimplifyVisitor(make_shared<SimplifyContext>("<generated>", ctx->cache))
        .transform(stmt);
  }
  return "." + typeName;
}

vector<StmtPtr> SimplifyVisitor::addMethods(const StmtPtr &s) {
  vector<StmtPtr> v;
  if (!s)
    return v;
  if (auto sp = CAST(s, SuiteStmt)) {
    for (auto &ss : sp->stmts)
      for (auto &u : addMethods(ss))
        v.push_back(move(u));
  } else if (CAST(s, ExprStmt) && CAST(CAST(s, ExprStmt)->expr, StringExpr)) {
  } else if (!CAST(s, FunctionStmt)) {
    error(s, "expected a function (only functions are allowed within type "
             "definitions)");
  } else {
    v.push_back(transform(s));
  }
  return v;
}

StmtPtr SimplifyVisitor::codegenMagic(const string &op, const ExprPtr &typExpr,
                                      const vector<Param> &args, bool isRecord) {
#define I(s) N<IdExpr>(s)
  ExprPtr ret;
  vector<Param> fargs;
  vector<StmtPtr> stmts;
  vector<string> attrs;
  if (op == "new") {
    ret = clone(typExpr);
    if (isRecord)
      for (auto &a : args)
        fargs.emplace_back(
            Param{a.name, clone(a.type),
                  a.deflt ? clone(a.deflt) : N<CallExpr>(clone(a.type))});
    attrs.emplace_back(ATTR_INTERNAL);
  } else if (op == "init") {
    assert(!isRecord);
    ret = I("void");
    fargs.emplace_back(Param{"self", clone(typExpr)});
    for (auto &a : args) {
      stmts.push_back(N<AssignMemberStmt>(I("self"), a.name, I(a.name)));
      fargs.emplace_back(Param{a.name, clone(a.type),
                               a.deflt ? clone(a.deflt) : N<CallExpr>(clone(a.type))});
    }
  } else if (op == "raw") {
    fargs.emplace_back(Param{"self", clone(typExpr)});
    ret = N<IndexExpr>(N<IdExpr>("Ptr"), N<IdExpr>("byte"));
    attrs.emplace_back(ATTR_INTERNAL);
  } else if (op == "getitem") {
    fargs.emplace_back(Param{"self", clone(typExpr)});
    fargs.emplace_back(Param{"index", I("int")});
    ret = !args.empty() ? clone(args[0].type) : I("void");
    attrs.emplace_back(ATTR_INTERNAL);
  } else if (op == "iter") {
    fargs.emplace_back(Param{"self", clone(typExpr)});
    ret = N<IndexExpr>(I("Generator"), !args.empty() ? clone(args[0].type) : I("void"));
    for (auto &a : args)
      stmts.emplace_back(N<YieldStmt>(N<DotExpr>(N<IdExpr>("self"), a.name)));
  } else if (op == "eq") {
    fargs.emplace_back(Param{"self", clone(typExpr)});
    fargs.emplace_back(Param{"other", clone(typExpr)});
    ret = I("bool");
    for (auto &a : args)
      stmts.push_back(N<IfStmt>(
          N<UnaryExpr>("!",
                       N<CallExpr>(N<DotExpr>(N<DotExpr>(I("self"), a.name), "__eq__"),
                                   N<DotExpr>(I("other"), a.name))),
          N<ReturnStmt>(N<BoolExpr>(false))));
    stmts.emplace_back(N<ReturnStmt>(N<BoolExpr>(true)));
  } else if (op == "ne") {
    fargs.emplace_back(Param{"self", clone(typExpr)});
    fargs.emplace_back(Param{"other", clone(typExpr)});
    ret = I("bool");
    for (auto &a : args)
      stmts.emplace_back(
          N<IfStmt>(N<CallExpr>(N<DotExpr>(N<DotExpr>(I("self"), a.name), "__ne__"),
                                N<DotExpr>(I("other"), a.name)),
                    N<ReturnStmt>(N<BoolExpr>(true))));
    stmts.push_back(N<ReturnStmt>(N<BoolExpr>(false)));
  } else if (op == "lt" || op == "gt") {
    fargs.emplace_back(Param{"self", clone(typExpr)});
    fargs.emplace_back(Param{"other", clone(typExpr)});
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
    fargs.emplace_back(Param{"self", clone(typExpr)});
    fargs.emplace_back(Param{"other", clone(typExpr)});
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
    fargs.emplace_back(Param{"self", clone(typExpr)});
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
    attrs.emplace_back("delay");
  } else if (op == "pickle") {
    fargs.emplace_back(Param{"self", clone(typExpr)});
    fargs.emplace_back(
        Param{"dest", N<IndexExpr>(N<IdExpr>("Ptr"), N<IdExpr>("byte"))});
    ret = I("void");
    for (auto &a : args)
      stmts.emplace_back(N<ExprStmt>(N<CallExpr>(
          N<DotExpr>(N<DotExpr>(I("self"), a.name), "__pickle__"), I("dest"))));
    attrs.emplace_back("delay");
  } else if (op == "unpickle") {
    fargs.emplace_back(Param{"src", N<IndexExpr>(N<IdExpr>("Ptr"), N<IdExpr>("byte"))});
    ret = clone(typExpr);
    vector<CallExpr::Arg> ar;
    for (auto &a : args)
      ar.emplace_back(CallExpr::Arg{
          "", N<CallExpr>(N<DotExpr>(clone(a.type), "__unpickle__"), I("src"))});
    stmts.emplace_back(N<ReturnStmt>(N<CallExpr>(clone(typExpr), move(ar))));
    attrs.emplace_back("delay");
  } else if (op == "len") {
    fargs.emplace_back(Param{"self", clone(typExpr)});
    ret = I("int");
    stmts.emplace_back(N<ReturnStmt>(N<IntExpr>(args.size())));
  } else if (op == "contains") {
    fargs.emplace_back(Param{"self", clone(typExpr)});
    fargs.emplace_back(Param{"what", args.size() ? clone(args[0].type) : I("void")});
    ret = I("bool");
    attrs.emplace_back(ATTR_INTERNAL);
  } else if (op == "to_py") {
    fargs.emplace_back(Param{"self", clone(typExpr)});
    ret = I("pyobj");
    stmts.emplace_back(
        N<AssignStmt>(I("o"), N<CallExpr>(N<DotExpr>(I("pyobj"), "_tuple_new"),
                                          N<IntExpr>(args.size()))));
    for (int i = 0; i < args.size(); i++)
      stmts.push_back(N<ExprStmt>(N<CallExpr>(
          N<DotExpr>(I("o"), "_tuple_set"), N<IntExpr>(i),
          N<CallExpr>(N<DotExpr>(N<DotExpr>(I("self"), args[i].name), "__to_py__")))));
    stmts.emplace_back(N<ReturnStmt>(I("o")));
    attrs.emplace_back("delay");
  } else if (op == "from_py") {
    fargs.emplace_back(Param{"src", I("pyobj")});
    ret = clone(typExpr);
    vector<CallExpr::Arg> ar;
    for (int i = 0; i < args.size(); i++)
      ar.push_back(CallExpr::Arg{
          "",
          N<CallExpr>(N<DotExpr>(clone(args[i].type), "__from_py__"),
                      N<CallExpr>(N<DotExpr>(I("src"), "_tuple_get"), N<IntExpr>(i)))});
    stmts.emplace_back(N<ReturnStmt>(N<CallExpr>(clone(typExpr), move(ar))));
    attrs.emplace_back("delay");
  } else if (op == "str") {
    fargs.emplace_back(Param{"self", clone(typExpr)});
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

ExprPtr SimplifyVisitor::transformGenericExpr(const ExprPtr &i) {
  auto t = transform(i, true);
  set<string> captures;
  if (isStaticExpr(t, captures))
    return N<StaticExpr>(clone(t), move(captures));
  else
    return t;
}

bool SimplifyVisitor::isStaticExpr(const ExprPtr &expr, set<string> &captures) {
  static unordered_set<string> supported{"<",  "<=", ">", ">=", "==", "!=", "&&",
                                         "||", "+",  "-", "*",  "//", "%"};
  if (auto ei = expr->getId()) {
    auto val = ctx->find(ei->value);
    if (val && val->isStatic()) {
      captures.insert(ei->value);
      return true;
    }
    return false;
  } else if (auto eb = expr->getBinary()) {
    return (supported.find(eb->op) != supported.end()) &&
           isStaticExpr(eb->lexpr, captures) && isStaticExpr(eb->rexpr, captures);
  } else if (auto eu = CAST(expr, UnaryExpr)) {
    return ((eu->op == "-") || (eu->op == "!")) && isStaticExpr(eu->expr, captures);
  } else if (auto ef = CAST(expr, IfExpr)) {
    return isStaticExpr(ef->cond, captures) && isStaticExpr(ef->ifexpr, captures) &&
           isStaticExpr(ef->elsexpr, captures);
  } else if (auto eit = expr->getInt()) {
    if (eit->suffix.size())
      return false;
    try {
      std::stoull(eit->value, nullptr, 0);
    } catch (std::out_of_range &) {
      return false;
    }
    return true;
  } else {
    return false;
  }
}

} // namespace ast
} // namespace seq