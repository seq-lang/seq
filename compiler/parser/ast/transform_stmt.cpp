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
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::unordered_set;
using std::vector;

namespace seq {
namespace ast {

using namespace types;

StmtPtr TransformVisitor::transform(const Stmt *stmt) {
  if (!stmt)
    return nullptr;

  TransformVisitor v(ctx);
  v.setSrcInfo(stmt->getSrcInfo());

  stmt->accept(v);
  if (v.prependStmts->size()) {
    if (v.resultStmt)
      v.prependStmts->push_back(move(v.resultStmt));
    v.resultStmt = N<SuiteStmt>(move(*v.prependStmts));
  }
  return move(v.resultStmt);
}

void TransformVisitor::visit(const SuiteStmt *stmt) {
  vector<StmtPtr> r;
  for (auto &s : stmt->stmts)
    if (auto t = transform(s))
      r.push_back(move(t));
  resultStmt = N<SuiteStmt>(move(r));
}

void TransformVisitor::visit(const PassStmt *stmt) { resultStmt = N<PassStmt>(); }

void TransformVisitor::visit(const BreakStmt *stmt) { resultStmt = N<BreakStmt>(); }

void TransformVisitor::visit(const ContinueStmt *stmt) {
  resultStmt = N<ContinueStmt>();
}

void TransformVisitor::visit(const ExprStmt *stmt) {
  resultStmt = N<ExprStmt>(transform(stmt->expr));
}

// Transformation
void TransformVisitor::visit(const AssignStmt *stmt) {
  vector<StmtPtr> stmts;
  if (stmt->type) {
    if (auto i = CAST(stmt->lhs, IdExpr))
      stmts.push_back(
          addAssignment(stmt->lhs.get(), stmt->rhs.get(), stmt->type.get()));
    else
      error("invalid type specifier");
  } else {
    processAssignment(stmt->lhs.get(), stmt->rhs.get(), stmts);
  }
  resultStmt = stmts.size() == 1 ? move(stmts[0]) : N<SuiteStmt>(move(stmts));
}

void TransformVisitor::visit(const UpdateStmt *stmt) {
  auto l = transform(stmt->lhs);
  auto r = transform(stmt->rhs);
  if (ctx->isTypeChecking())
    forceUnify(l->getType(), r->getType());
  resultStmt = N<UpdateStmt>(move(l), move(r));
}

void TransformVisitor::visit(const AssignMemberStmt *stmt) {
  auto lh = transform(stmt->lhs), rh = transform(stmt->rhs);
  if (ctx->isTypeChecking()) {
    auto c = lh->getType()->getClass();
    if (c && c->isRecord())
      error("records are read-only ^ {} , {}", c->toString(), lh->toString());
    auto mm = ctx->getRealizations()->findMember(c->name, stmt->member);
    // LOG9("lhs has type {}, un with {}", mm->toString(),
    // rh->getType()->toString());
    forceUnify(ctx->instantiate(getSrcInfo(), mm, c), rh->getType());
  }
  resultStmt = N<AssignMemberStmt>(move(lh), stmt->member, move(rh));
}

// Transformation
void TransformVisitor::visit(const DelStmt *stmt) {
  if (auto expr = CAST(stmt->expr, IndexExpr)) {
    resultStmt = N<ExprStmt>(transform(N<CallExpr>(
        N<DotExpr>(expr->expr->clone(), "__delitem__"), expr->index->clone())));
  } else if (auto expr = CAST(stmt->expr, IdExpr)) {
    ctx->remove(expr->value);
    resultStmt = N<DelStmt>(transform(expr));
  } else {
    error("expression cannot be deleted");
  }
}

// Transformation
void TransformVisitor::visit(const PrintStmt *stmt) {
  if (ctx->isTypeChecking())
    resultStmt = N<ExprStmt>(transform(N<CallExpr>(
        N<IdExpr>(".seq_print"), conditionalMagic(stmt->expr, "str", "__str__"))));
  else
    resultStmt = N<PrintStmt>(transform(stmt->expr));
}

// TODO check if in function!
void TransformVisitor::visit(const ReturnStmt *stmt) {
  if (!ctx->getLevel() || !ctx->bases.back().parent->getFunc())
    error("expected function body");
  if (stmt->expr) {
    auto e = transform(stmt->expr);
    if (ctx->isTypeChecking()) {
      auto &base = ctx->bases.back();
      if (base.returnType)
        forceUnify(e->getType(), base.returnType);
      else
        base.returnType = e->getType();
    }
    resultStmt = N<ReturnStmt>(move(e));
  } else {
    resultStmt = N<ReturnStmt>(nullptr);
  }
}

void TransformVisitor::visit(const YieldStmt *stmt) {
  if (!ctx->getLevel() || !ctx->bases.back().parent->getFunc())
    error("expected function body");
  types::TypePtr t = nullptr;
  if (stmt->expr) {
    auto e = transform(stmt->expr);
    if (ctx->isTypeChecking())
      t = ctx->instantiateGeneric(e->getSrcInfo(), ctx->findInternal("Generator"),
                                  {e->getType()});
    resultStmt = N<YieldStmt>(move(e));
  } else {
    if (ctx->isTypeChecking())
      t = ctx->instantiateGeneric(stmt->getSrcInfo(), ctx->findInternal("Generator"),
                                  {ctx->findInternal("void")});
    resultStmt = N<YieldStmt>(nullptr);
  }
  if (ctx->isTypeChecking()) {
    auto &base = ctx->bases.back();
    if (base.returnType)
      forceUnify(t, base.returnType);
    else
      base.returnType = t;
  }
}

void TransformVisitor::visit(const AssertStmt *stmt) {
  resultStmt = N<AssertStmt>(transform(stmt->expr));
}

void TransformVisitor::visit(const WhileStmt *stmt) {
  auto cond = makeBoolExpr(stmt->cond);
  ctx->addBlock();
  resultStmt = N<WhileStmt>(move(cond), transform(stmt->suite));
  ctx->popBlock();
}

void TransformVisitor::visit(const ForStmt *stmt) {
  auto iter = conditionalMagic(stmt->iter, "Generator", "__iter__");
  TypePtr varType = nullptr;
  if (ctx->isTypeChecking()) {
    varType = ctx->addUnbound(stmt->var->getSrcInfo(), ctx->getLevel());
    if (!iter->getType()->getUnbound()) {
      auto iterType = iter->getType()->getClass();
      if (!iterType || iterType->name != "Generator")
        error(iter, "expected a generator");
      forceUnify(varType, iterType->explicits[0].type);
    }
  }

  ctx->addBlock();
  if (auto i = CAST(stmt->var, IdExpr)) {
    string varName = i->value;
    ctx->addVar(varName, varType);
    resultStmt = N<ForStmt>(transform(stmt->var), move(iter), transform(stmt->suite));
  } else {
    string varName = getTemporaryVar("for");
    ctx->addVar(varName, varType);
    auto var = N<IdExpr>(varName);
    vector<StmtPtr> stmts;
    stmts.push_back(N<AssignStmt>(stmt->var->clone(), var->clone(), nullptr, false,
                                  /* force */ true));
    stmts.push_back(stmt->suite->clone());
    resultStmt =
        N<ForStmt>(var->clone(), move(iter), transform(N<SuiteStmt>(move(stmts))));
  }
  ctx->popBlock();
}

void TransformVisitor::visit(const IfStmt *stmt) {
  vector<IfStmt::If> ifs;
  for (auto &i : stmt->ifs) {
    auto cond = i.cond ? makeBoolExpr(i.cond) : nullptr;
    ctx->addBlock();
    ifs.push_back({move(cond), transform(i.suite)});
    ctx->popBlock();
  }
  resultStmt = N<IfStmt>(move(ifs));
}

void TransformVisitor::visit(const MatchStmt *stmt) {
  auto w = transform(stmt->what);
  ctx->setMatchType(w->getType());
  vector<PatternPtr> patterns;
  vector<StmtPtr> cases;
  for (auto ci = 0; ci < stmt->cases.size(); ci++) {
    string varName;
    if (auto p = CAST(stmt->patterns[ci], BoundPattern)) {
      ctx->addBlock();
      auto boundPat = transform(p->pattern);
      ctx->addVar(p->var, boundPat->getType());
      patterns.push_back(move(boundPat));
      cases.push_back(transform(stmt->cases[ci]));
      ctx->popBlock();
    } else {
      ctx->addBlock();
      patterns.push_back(transform(stmt->patterns[ci]));
      cases.push_back(transform(stmt->cases[ci]));
      ctx->popBlock();
    }
  }
  ctx->setMatchType(nullptr);
  resultStmt = N<MatchStmt>(move(w), move(patterns), move(cases));
}

void TransformVisitor::visit(const ExtendStmt *stmt) {
  TypePtr type;
  vector<string> generics;

  auto getType = [&](const ExprPtr &e) -> TypePtr {
    if (auto i = CAST(e, IdExpr)) {
      auto val = processIdentifier(ctx, i->value);
      if (auto t = val->getType())
        return t;
    }
    error("invalid generic identifier");
    return nullptr;
  };
  if (auto e = CAST(stmt->what, IndexExpr)) {
    type = getType(e->expr);
    if (auto t = CAST(e->index, TupleExpr))
      for (auto &ti : t->items) {
        if (auto s = CAST(ti, IdExpr))
          generics.push_back(s->value);
        else
          error(ti, "invalid generic identifier");
      }
    else if (auto i = CAST(e->index, IdExpr))
      generics.push_back(i->value);
    else
      error(e->index, "invalid generic identifier");
  } else {
    type = getType(stmt->what);
  }
  auto c = type->getClass();
  assert(c);
  auto canonicalName = c->name;
  if (c->explicits.size() != generics.size())
    error("expected {} generics, got {}", c->explicits.size(), generics.size());

  ctx->bases.push_back({c});
  ctx->bases.back().parentAst = stmt->what->clone();
  for (int i = 0; i < generics.size(); i++) {
    auto l = c->explicits[i].type->getLink();
    assert(l);
    ctx->addType(generics[i],
                 ctx->isTypeChecking()
                     ? make_shared<LinkType>(LinkType::Unbound, c->explicits[i].id,
                                             ctx->getLevel() - 1, nullptr, l->isStatic)
                     : nullptr,
                 true);
  }
  vector<StmtPtr> funcStmts;
  for (auto s : stmt->suite->getStatements())
    funcStmts.push_back(addMethod(s, canonicalName));
  ctx->bases.pop_back();

  for (int i = 0; i < generics.size(); i++) {
    if (ctx->isTypeChecking() && c->explicits[i].type) {
      auto t =
          dynamic_pointer_cast<LinkType>(ctx->find(generics[i])->getType()->follow());
      assert(t && t->kind == LinkType::Unbound);
      t->kind = LinkType::Generic;
    }
    ctx->remove(generics[i]);
  }

  LOG7("[extnd] {}", c->toString());
  for (auto &m : ctx->getRealizations()->classes[canonicalName].members)
    LOG7("       - member: {}: {}", m.first, m.second->toString());
  for (auto &m : ctx->getRealizations()->classes[canonicalName].methods)
    for (auto &f : m.second) {
      if (ctx->isTypeChecking() && f->canRealize())
        realizeFunc(ctx->instantiate(getSrcInfo(), f)->getFunc());
      LOG7("       - method: {}: {}", m.first, f->toString());
    }

  resultStmt = N<SuiteStmt>(move(funcStmts));
}

void TransformVisitor::visit(const ImportStmt *stmt) {
  auto file = stmt->from.first;
  if (file.size() && file[0] == '/')
    file = file.substr(1);
  else
    file = ctx->getImports()->getImportFile(stmt->from.first, ctx->getFilename());
  if (file.empty())
    error("cannot locate import '{}'", stmt->from.first);

  auto import = ctx->getImports()->getImport(file);
  if (!import) {
    auto ictx =
        make_shared<TypeContext>(file, ctx->getRealizations(), ctx->getImports());
    ctx->getImports()->addImport(file, file, ictx);

    auto s = parseFile(file);
    auto sn = TransformVisitor(ictx).realizeBlock(s.get(), true);
    ctx->getImports()->setBody(file, move(sn));
    import = ctx->getImports()->getImport(file);
  }

  auto addRelated = [&](string n) {
    /// TODO switch to map maybe to make this more efficient?
    /// Check are there any clashes? Only parse "."-prefixed items?
    for (auto i : *(import->tctx)) {
      if (i.first.substr(0, n.size()) == n)
        ctx->add(i.first, i.second.front());
    }
  };

  if (!stmt->what.size()) {
    ctx->addImport(stmt->from.second == "" ? stmt->from.first : stmt->from.second,
                   file);
  } else if (stmt->what.size() == 1 && stmt->what[0].first == "*") {
    if (stmt->what[0].second != "")
      error("cannot rename star-import");
    for (auto &i : *(import->tctx))
      ctx->add(i.first, i.second.front());
  } else {
    for (auto &w : stmt->what) {
      auto c = import->tctx->find(w.first);
      if (!c)
        error("symbol '{}' not found in {}", w.first, file);
      ctx->add(w.second == "" ? w.first : w.second, c);
      if (c->getClass())
        addRelated(c->getType()->getClass()->name);
      else if (c->getFunc())
        addRelated(c->getType()->getFunc()->name);
    }
  }

  resultStmt = N<ImportStmt>(make_pair("/" + file, stmt->from.second), stmt->what);
}

// Transformation
void TransformVisitor::visit(const ExternImportStmt *stmt) {
  if (stmt->lang == "c" && stmt->from) {
    vector<StmtPtr> stmts;
    // ptr = _dlsym(FROM, WHAT)
    stmts.push_back(N<AssignStmt>(N<IdExpr>("Ptr"),
                                  N<CallExpr>(N<IdExpr>("_dlsym"), stmt->from->clone(),
                                              N<StringExpr>(stmt->name.first))));
    // f = function[ARGS](ptr)
    vector<ExprPtr> args;
    args.push_back(stmt->ret ? stmt->ret->clone() : N<IdExpr>("void"));
    for (auto &a : stmt->args)
      args.push_back(a.type->clone());
    stmts.push_back(N<AssignStmt>(
        N<IdExpr>("f"),
        N<CallExpr>(N<IndexExpr>(N<IdExpr>("Function"), N<TupleExpr>(move(args))),
                    N<IdExpr>("Ptr"))));
    bool isVoid = true;
    if (stmt->ret) {
      if (auto f = CAST(stmt->ret, IdExpr))
        isVoid = f->value == "void";
      else
        isVoid = false;
    }
    args.clear();
    for (int i = 0; i < stmt->args.size(); i++)
      args.push_back(
          N<IdExpr>(stmt->args[i].name != "" ? stmt->args[i].name : format("$a{}", i)));
    // return f(args)
    auto call = N<CallExpr>(N<IdExpr>("f"), move(args));
    if (!isVoid)
      stmts.push_back(N<ReturnStmt>(move(call)));
    else
      stmts.push_back(N<ExprStmt>(move(call)));
    // def WHAT(args):
    vector<Param> params;
    for (int i = 0; i < stmt->args.size(); i++)
      params.push_back(
          {stmt->args[i].name != "" ? stmt->args[i].name : format("$a{}", i),
           stmt->args[i].type->clone()});
    resultStmt = transform(
        N<FunctionStmt>(stmt->name.second != "" ? stmt->name.second : stmt->name.first,
                        stmt->ret->clone(), vector<Param>(), move(params),
                        N<SuiteStmt>(move(stmts)), vector<string>()));
  } else if (stmt->lang == "c") {
    auto canonicalName = ctx->getRealizations()->generateCanonicalName(
        stmt->getSrcInfo(), format("{}.{}", ctx->getBase(), stmt->name.first));
    if (!stmt->ret)
      error("expected return type");
    vector<Param> args;
    vector<TypePtr> argTypes{};

    auto v = ctx->findInternal(generateFunctionStub(stmt->args.size() + 1))->getClass();
    auto t = make_shared<FuncType>(canonicalName, v);
    t->setSrcInfo(stmt->getSrcInfo());
    t->args.push_back(transformType(stmt->ret)->getType());
    for (int ai = 0; ai < stmt->args.size(); ai++) {
      if (stmt->args[ai].deflt)
        error("default arguments not supported here");
      args.push_back(
          {stmt->args[ai].name, transformType(stmt->args[ai].type), nullptr});
      t->args.push_back(args.back().type->getType());
    }
    t = std::static_pointer_cast<FuncType>(t->generalize(ctx->getLevel()));

    // do not add class member
    if (!ctx->getLevel() || ctx->bases.back().parent->getFunc())
      ctx->addFunc(stmt->name.second != "" ? stmt->name.second : stmt->name.first, t);
    ctx->addGlobal(canonicalName, t);
    ctx->getRealizations()->funcASTs[canonicalName] =
        make_pair(t, N<FunctionStmt>(stmt->name.first, nullptr, vector<Param>(),
                                     move(args), nullptr, vector<string>{"$external"}));
    resultStmt = N<FunctionStmt>(stmt->name.first, nullptr, vector<Param>(),
                                 vector<Param>(), nullptr, vector<string>{"$external"});
  } else if (stmt->lang == "py") {
    vector<StmtPtr> stmts;
    string from = "";
    if (auto i = CAST(stmt->from, IdExpr))
      from = i->value;
    else
      error("invalid pyimport query");
    auto call = N<CallExpr>( // _py_import(LIB)[WHAT].call (x.__to_py__)
        N<DotExpr>(
            N<IndexExpr>(N<CallExpr>(N<IdExpr>("_py_import"), N<StringExpr>(from)),
                         N<StringExpr>(stmt->name.first)),
            "call"),
        N<CallExpr>(N<DotExpr>(N<IdExpr>("x"), "__to_py__")));
    bool isVoid = true;
    if (stmt->ret) {
      if (auto f = CAST(stmt->ret, IdExpr))
        isVoid = f->value == "void";
      else
        isVoid = false;
    }
    if (!isVoid) // return TYP.__from_py__(call)
      stmts.push_back(N<ReturnStmt>(
          N<CallExpr>(N<DotExpr>(stmt->ret->clone(), "__from_py__"), move(call))));
    else
      stmts.push_back(N<ExprStmt>(move(call)));
    vector<Param> params;
    params.push_back({"x", nullptr, nullptr});
    resultStmt = transform(
        N<FunctionStmt>(stmt->name.second != "" ? stmt->name.second : stmt->name.first,
                        stmt->ret->clone(), vector<Param>(), move(params),
                        N<SuiteStmt>(move(stmts)), vector<string>{"pyhandle"}));
  } else {
    error("language '{}' not supported", stmt->lang);
  }
}

void TransformVisitor::visit(const TryStmt *stmt) {
  vector<TryStmt::Catch> catches;
  ctx->addBlock();
  auto suite = transform(stmt->suite);
  ctx->popBlock();
  for (auto &c : stmt->catches) {
    ctx->addBlock();
    auto exc = transformType(c.exc);
    if (c.var != "")
      ctx->addVar(c.var, exc->getType());
    catches.push_back({c.var, move(exc), transform(c.suite)});
    ctx->popBlock();
  }
  ctx->addBlock();
  auto finally = transform(stmt->finally);
  ctx->popBlock();
  resultStmt = N<TryStmt>(move(suite), move(catches), move(finally));
}

void TransformVisitor::visit(const GlobalStmt *stmt) {
  if (ctx->getBase() == "")
    error("'global' is only applicable within function blocks");
  auto val = ctx->find(stmt->var);
  if (!val || !val->getVar())
    error("identifier '{}' not found", stmt->var);
  if (val->getBase() != "")
    error("not a toplevel variable");
  if (!val->getVar()->getType()->canRealize())
    error("global variables must have realized types");
  val->setGlobal();
  ctx->addVar(stmt->var, val->getVar()->getType());
  resultStmt = N<GlobalStmt>(stmt->var);
}

void TransformVisitor::visit(const ThrowStmt *stmt) {
  resultStmt = N<ThrowStmt>(transform(stmt->expr));
}

void TransformVisitor::visit(const FunctionStmt *stmt) {
  auto canonicalName = ctx->getRealizations()->generateCanonicalName(
      stmt->getSrcInfo(), format("{}.{}", ctx->getBase(), stmt->name));
  bool isClassMember = ctx->getLevel() && !ctx->bases.back().parent->getFunc();
  resultStmt = N<FunctionStmt>(stmt->name, nullptr, vector<Param>(), vector<Param>(),
                               nullptr, stmt->attributes);
  if (ctx->getRealizations()->funcASTs.find(canonicalName) !=
      ctx->getRealizations()->funcASTs.end()) {
    if (!isClassMember)
      ctx->addFunc(stmt->name, ctx->getRealizations()->funcASTs[canonicalName].first);
    return;
  }
  LOG7("[stmt] adding func {} (base: {})", canonicalName, ctx->getBase());

  vector<Param> args;
  auto v = ctx->findInternal(generateFunctionStub(stmt->args.size() + 1))->getClass();
  // If type checking is not active, make all arguments generic
  auto t = make_shared<FuncType>(canonicalName, v);
  ctx->bases.push_back({t});
  t->explicits =
      parseGenerics(stmt->generics, ctx->getLevel() - 1); // generics are level down
  vector<TypePtr> generics;
  for (auto &i : stmt->generics)
    generics.push_back(ctx->find(i.name)->getType());
  if (stmt->ret && ctx->isTypeChecking()) {
    t->args.push_back(transformType(stmt->ret)->getType());
  } else {
    t->args.push_back(ctx->addUnbound(getSrcInfo(), ctx->getLevel()));
    generics.push_back(t->args.back());
  }
  for (int ia = 0; ia < stmt->args.size(); ia++) {
    auto &a = stmt->args[ia];
    ExprPtr typeAst = nullptr;
    if (ctx->isTypeChecking() && isClassMember && ia == 0 && !a.type &&
        a.name == "self") {
      typeAst = transformType(ctx->bases[ctx->bases.size() - 2].parentAst->clone());
      t->args.push_back(typeAst->getType());
    } else if (ctx->isTypeChecking() && a.type) {
      typeAst = transformType(a.type);
      t->args.push_back(typeAst->getType());
    } else {
      t->args.push_back(ctx->addUnbound(getSrcInfo(), ctx->getLevel()));
      generics.push_back(t->args.back());
    }
    args.push_back({a.name, move(typeAst), a.deflt ? a.deflt->clone() : nullptr});
    ctx->addVar(a.name, t->args.back());
  }
  auto suite = stmt->suite ? stmt->suite->clone() : nullptr;
  if (!in(stmt->attributes, "internal")) {
    TransformVisitor v(ctx);
    ctx->addBlock();
    LOG7("=== BEFORE === {} \n{}", canonicalName, suite->toString());
    auto oldTC = ctx->isTypeChecking();
    ctx->typecheck = false;
    suite = v.transform(stmt->suite.get());
    ctx->popBlock();
    LOG7("=== AFTER === {} \n{}", canonicalName, suite->toString());
    ctx->typecheck = oldTC;
  }
  auto ref = ctx->bases.back().referencesParent;
  ctx->bases.pop_back();
  t->parent = ctx->bases.back().parent;
  if (isClassMember && !ref && canonicalName != ".Ptr.__elemsize__" &&
      canonicalName != ".Ptr.__atomic__") {
    t->codegenParent = t->parent;
    t->parent =
        ctx->bases.size() > 1 ? ctx->bases[ctx->bases.size() - 2].parent : nullptr;
  }

  // Generalize generics
  // Ensure that implicit "generics" are also generalized
  for (auto g : generics) {
    assert(g && g->getLink() && g->getLink()->kind != types::LinkType::Link);
    if (g->getLink()->kind == LinkType::Unbound)
      g->getLink()->kind = LinkType::Generic;
  }
  for (auto &g : stmt->generics)
    ctx->remove(g.name);

  t->setSrcInfo(stmt->getSrcInfo());
  t = std::static_pointer_cast<FuncType>(t->generalize(ctx->getLevel()));
  if (!isClassMember)
    ctx->addFunc(stmt->name, t);
  ctx->addGlobal(canonicalName, t);
  LOG7("[stmt] added func {}: {} (@{}; ref={})", canonicalName, t->toString(),
       t->parent ? t->parent->toString() : "-", ref);
  ctx->getRealizations()->funcASTs[canonicalName] =
      make_pair(t, N<FunctionStmt>(stmt->name, nullptr, CL(stmt->generics), move(args),
                                   move(suite), stmt->attributes));

  if (in(stmt->attributes, "builtin")) {
    if (!t->canRealize())
      error("builtins must be realizable");
    if (ctx->getLevel() || isClassMember)
      error("builtins must be defined at the toplevel");
  }
  // Class members are realized after the class is sealed to prevent premature
  // unification of class generics
  if (!isClassMember && ctx->isTypeChecking() && t->canRealize())
    realizeFunc(ctx->instantiate(getSrcInfo(), t)->getFunc());
}

void TransformVisitor::visit(const ClassStmt *stmt) {
  auto canonicalName = ctx->getRealizations()->generateCanonicalName(
      stmt->getSrcInfo(), format("{}.{}", ctx->getBase(), stmt->name));
  resultStmt = N<ClassStmt>(stmt->isRecord, stmt->name, vector<Param>(),
                            vector<Param>(), N<SuiteStmt>(), stmt->attributes);
  auto cit = ctx->getRealizations()->classASTs.find(canonicalName);
  if (cit != ctx->getRealizations()->classASTs.end()) {
    ctx->addType(stmt->name, cit->second);
    return;
  }

  LOG7("[stmt] adding type {} (base: {})", canonicalName, ctx->getBase());
  vector<StmtPtr> stmts;
  stmts.push_back(move(resultStmt));

  auto ct = make_shared<ClassType>(
      canonicalName, stmt->isRecord, vector<TypePtr>(), vector<Generic>(),
      ctx->getLevel() ? ctx->bases.back().parent : nullptr);
  ct->setSrcInfo(stmt->getSrcInfo());
  auto ctxi =
      make_shared<TypeItem::Class>(ct, false, ctx->getFilename(), ctx->getBase(), true);
  if (!stmt->isRecord) { // add classes early
    ctx->add(stmt->name, ctxi);
    ctx->addGlobal(canonicalName, ct);
  }
  ctx->getRealizations()->classASTs[canonicalName] = ct;

  ctx->bases.push_back({ct});
  ct->explicits = parseGenerics(stmt->generics, ctx->getLevel() - 1);
  LOG7("[stmt] added class {}: {}", canonicalName, ct->toString());

  unordered_set<string> seenMembers;
  ExprPtr mainType = nullptr;
  for (auto &a : stmt->args) {
    assert(a.type);
    if (!mainType)
      mainType = a.type->clone();
    auto t = transformType(a.type)->getType()->generalize(ctx->getLevel() - 1);
    // LOG7("{} : {} -> {} # {}", ct->toString(), a.name, t->toString(),
    //      a.type->toString());
    ctx->getRealizations()->classes[canonicalName].members.push_back({a.name, t});
    if (seenMembers.find(a.name) != seenMembers.end())
      error(a.type, "{} declared twice", a.name);
    seenMembers.insert(a.name);
    if (stmt->isRecord)
      ct->args.push_back(t);
  }
  if (!mainType)
    mainType = N<IdExpr>("void");
  if (stmt->isRecord) {
    ctx->add(stmt->name, ctxi);
    ctx->addGlobal(canonicalName, ct);
  }
  vector<ExprPtr> genAst;
  for (auto &g : stmt->generics)
    genAst.push_back(N<IdExpr>(g.name));
  ctx->bases.back().parentAst =
      genAst.size()
          ? (ExprPtr)N<IndexExpr>(N<IdExpr>(stmt->name), N<TupleExpr>(move(genAst)))
          : (ExprPtr)N<IdExpr>(stmt->name);
  if (!in(stmt->attributes, "internal")) {
    vector<ExprPtr> genericNames;
    for (auto &g : stmt->generics)
      genericNames.push_back(N<IdExpr>(g.name));
    ExprPtr codeType = N<IdExpr>(stmt->name);
    if (genericNames.size())
      codeType = N<IndexExpr>(move(codeType), N<TupleExpr>(move(genericNames)));
    vector<Param> args;
    if (!stmt->isRecord)
      args.push_back(Param{"self"});
    for (auto &a : stmt->args)
      args.push_back(Param{a.name, a.type->clone()});
    vector<StmtPtr> fns;
    bool empty = canonicalName == ".tuple.0";
    if (!stmt->isRecord) {
      fns.push_back(makeInternalFn("__new__", codeType->clone()));
      fns.push_back(makeInternalFn("__init__", N<IdExpr>("void"), move(args)));
      fns.push_back(makeInternalFn("__bool__", N<IdExpr>("bool"), Param{"self"}));
      fns.push_back(makeInternalFn("__pickle__", N<IdExpr>("void"), Param{"self"},
                                   Param{"dest", N<IdExpr>("cobj")}));
      fns.push_back(makeInternalFn("__unpickle__", codeType->clone(),
                                   Param{"src", N<IdExpr>("cobj")}));
      fns.push_back(makeInternalFn("__raw__", N<IdExpr>("cobj"), Param{"self"}));
    } else {
      fns.push_back(makeInternalFn("__new__", codeType->clone(), move(args)));
      fns.push_back(makeInternalFn("__str__", N<IdExpr>("str"), Param{"self"}));
      fns.push_back(makeInternalFn("__len__", N<IdExpr>("int"), Param{"self"}));
      fns.push_back(makeInternalFn("__hash__", N<IdExpr>("int"), Param{"self"}));
      fns.push_back(makeInternalFn(
          "__iter__", N<IndexExpr>(N<IdExpr>("Generator"), N<IdExpr>("int")),
          Param{"self"}));
      fns.push_back(makeInternalFn("__pickle__", N<IdExpr>("void"), Param{"self"},
                                   Param{"dest", N<IdExpr>("cobj")}));
      fns.push_back(makeInternalFn("__unpickle__", codeType->clone(),
                                   Param{"src", N<IdExpr>("cobj")}));
      fns.push_back(makeInternalFn("__getitem__",
                                   empty ? N<IdExpr>("void") : mainType->clone(),
                                   Param{"self"}, Param{"index", N<IdExpr>("int")}));
      if (!empty)
        fns.push_back(makeInternalFn("__contains__", N<IdExpr>("bool"), Param{"self"},
                                     Param{"what", mainType->clone()}));
      fns.push_back(makeInternalFn("__to_py__", N<IdExpr>("pyobj"), Param{"self"}));
      fns.push_back(makeInternalFn("__from_py__", codeType->clone(),
                                   Param{"src", N<IdExpr>("pyobj")}));
      for (auto &m : {"__eq__", "__ne__", "__lt__", "__gt__", "__le__", "__ge__"})
        fns.push_back(makeInternalFn(m, N<IdExpr>("bool"), Param{"self"},
                                     Param{"what", codeType->clone()}));
    }
    for (auto &s : fns)
      stmts.push_back(addMethod(s.get(), canonicalName));
  }
  for (auto s : stmt->suite->getStatements())
    stmts.push_back(addMethod(s, canonicalName));
  ctx->bases.pop_back();

  // Generalize in place
  for (auto &g : stmt->generics) {
    auto val = ctx->find(g.name);
    if (auto g = val->getType()) {
      assert(g && g->getLink() && g->getLink()->kind != types::LinkType::Link);
      if (g->getLink()->kind == LinkType::Unbound)
        g->getLink()->kind = LinkType::Generic;
    }
    ctx->remove(g.name);
  }

  LOG7("[class] {}", ct->toString());
  for (auto &m : ctx->getRealizations()->classes[canonicalName].members)
    LOG7("       - member: {}: {}", m.first, m.second->toString());
  for (auto &m : ctx->getRealizations()->classes[canonicalName].methods)
    for (auto &f : m.second) {
      if (ctx->isTypeChecking() && f->canRealize())
        realizeFunc(ctx->instantiate(getSrcInfo(), f)->getFunc());
      LOG7("       - method: {}: {}", m.first, f->toString());
    }

  resultStmt = N<SuiteStmt>(move(stmts));
}

// Transformation
void TransformVisitor::visit(const AssignEqStmt *stmt) {
  resultStmt = transform(N<AssignStmt>(
      stmt->lhs->clone(),
      N<BinaryExpr>(stmt->lhs->clone(), stmt->op, stmt->rhs->clone(), true), nullptr,
      true));
}

// Transformation
void TransformVisitor::visit(const YieldFromStmt *stmt) {
  auto var = getTemporaryVar("yield");
  resultStmt = transform(
      N<ForStmt>(N<IdExpr>(var), stmt->expr->clone(), N<YieldStmt>(N<IdExpr>(var))));
}

// Transformation
void TransformVisitor::visit(const WithStmt *stmt) {
  assert(stmt->items.size());
  vector<StmtPtr> content;
  for (int i = stmt->items.size() - 1; i >= 0; i--) {
    vector<StmtPtr> internals;
    string var = stmt->vars[i] == "" ? getTemporaryVar("with") : stmt->vars[i];
    internals.push_back(N<AssignStmt>(N<IdExpr>(var), stmt->items[i]->clone()));
    internals.push_back(
        N<ExprStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "__enter__"))));
    internals.push_back(
        N<TryStmt>(content.size() ? N<SuiteStmt>(move(content)) : stmt->suite->clone(),
                   vector<TryStmt::Catch>{},
                   N<SuiteStmt>(N<ExprStmt>(
                       N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "__exit__"))))));
    content = move(internals);
  }
  resultStmt = transform(N<IfStmt>(N<BoolExpr>(true), N<SuiteStmt>(move(content))));
}

// Transformation
void TransformVisitor::visit(const PyDefStmt *stmt) {
  // _py_exec(""" str """)
  vector<string> args;
  for (auto &a : stmt->args)
    args.push_back(a.name);
  string code =
      format("def {}({}):\n{}\n", stmt->name, fmt::join(args, ", "), stmt->code);
  resultStmt = transform(
      N<SuiteStmt>(N<ExprStmt>(N<CallExpr>(N<IdExpr>("_py_exec"), N<StringExpr>(code))),
                   // from __main__ pyimport foo () -> ret
                   N<ExternImportStmt>(make_pair(stmt->name, ""), N<IdExpr>("__main__"),
                                       stmt->ret->clone(), vector<Param>(), "py")));
}

} // namespace ast
} // namespace seq
