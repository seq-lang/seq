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

vector<types::Generic> TransformVisitor::parseGenerics(const vector<Param> &generics,
                                                       int level) {
  auto genericTypes = vector<types::Generic>();
  for (auto &g : generics) {
    assert(!g.name.empty());
    if (g.type && g.type->toString() != "(#id int)")
      error("only int generic types are allowed");
    auto tp = ctx->addUnbound(getSrcInfo(), level, true, bool(g.type));
    if (!ctx->isTypeChecking())
      tp = tp->generalize(level);
    genericTypes.push_back(
        {g.name, tp->generalize(level), ctx->getRealizations()->getUnboundCount() - 1});
    LOG7("[generic] {} -> {}", g.name, tp->toString(0));
    if (g.type)
      ctx->addStatic(g.name, 0, tp);
    else
      ctx->addType(g.name, tp, true);
  }
  return genericTypes;
}

StmtPtr TransformVisitor::addMethod(Stmt *s, const string &canonicalName) {
  if (auto f = dynamic_cast<FunctionStmt *>(s)) {
    auto fs = transform(f);
    auto name = ctx->getRealizations()->getCanonicalName(f->getSrcInfo());
    auto val = ctx->find(name);
    assert(val);
    auto fv = val->getType()->getFunc();
    LOG9("[add_method] {} ... {}", name, val->getType()->toString());
    assert(fv);
    ctx->getRealizations()->classes[canonicalName].methods[f->name].push_back(fv);
    return fs;
  } else {
    error(s, "expected a function (only functions are allowed within type "
             "definitions)");
    return nullptr;
  }
}

string TransformVisitor::generateFunctionStub(int len) {
  assert(len >= 1);
  auto typeName = fmt::format(".Function.{}",
                              len - 1); // dot needed here as we manually insert class
  if (ctx->getRealizations()->variardicCache.find(typeName) !=
      ctx->getRealizations()->variardicCache.end())
    return typeName;

  ctx->getRealizations()->variardicCache.insert(typeName);
  auto stdlib = ctx->getImports()->getImport("");
  auto oldTC = stdlib->tctx->isTypeChecking();
  stdlib->tctx->typecheck = true;

  vector<Param> generics, args;
  vector<ExprPtr> genericNames;
  for (int i = 1; i <= len; i++) {
    genericNames.push_back(N<IdExpr>(format("T{}", i)));
    generics.push_back(Param{format("T{}", i), nullptr, nullptr});
    args.push_back(Param{format("a{0}", i), N<IdExpr>(format("T{}", i)), nullptr});
  }
  ExprPtr type = N<IdExpr>(typeName);
  if (genericNames.size())
    type = N<IndexExpr>(move(type), N<TupleExpr>(move(genericNames)));
  auto stmt = make_unique<ClassStmt>(true, typeName, move(generics), CL(args), nullptr,
                                     vector<string>{});
  stmt->setSrcInfo(ctx->getRealizations()->getGeneratedPos());
  auto ct = make_shared<ClassType>(typeName, true, vector<TypePtr>(), vector<Generic>(),
                                   nullptr);
  ct->setSrcInfo(stmt->getSrcInfo());
  ctx->getRealizations()->classASTs[typeName] = ct;

  auto nc = make_shared<TypeContext>(stdlib->tctx->getFilename(),
                                     stdlib->tctx->getRealizations(),
                                     stdlib->tctx->getImports());
  nc->bases.push_back({ct});
  nc->bases.back().parentAst = type->clone();
  auto tv = TransformVisitor(nc);

  ct->explicits = tv.parseGenerics(stmt->generics, nc->getLevel() - 1);
  for (auto &a : args) {
    auto t = tv.transformType(a.type)->getType()->generalize(nc->getLevel() - 1);
    nc->getRealizations()->classes[typeName].members.push_back({a.name, t});
    ct->args.push_back(t);
  }
  nc->addGlobal(typeName, ct);

  vector<StmtPtr> fns;
  fns.push_back(
      tv.makeInternalFn("__new__", type->clone(), Param{"what", N<IdExpr>("cobj")}));
  fns.push_back(tv.makeInternalFn("__str__", N<IdExpr>("str"), Param{"self"}));
  stmt->attributes.push_back("internal");
  auto suite = N<SuiteStmt>();
  for (auto &s : fns)
    suite->stmts.push_back(tv.addMethod(s.get(), typeName));
  nc->bases.pop_back();
  for (auto &g : stmt->generics) {
    auto val = nc->find(g.name);
    if (nc->isTypeChecking())
      if (auto tx = val->getType()) {
        auto t = dynamic_pointer_cast<LinkType>(tx);
        assert(t && t->kind == LinkType::Unbound);
        t->kind = LinkType::Generic;
      }
    nc->remove(g.name);
  }
  stdlib->tctx->typecheck = oldTC;

  stmt->suite = move(suite);
  dynamic_cast<SuiteStmt *>(stdlib->statements.get())->stmts.push_back(move(stmt));
  LOG7("[var] generated {}: {}", typeName, ct->toString());
  LOG7("[class] {}", ct->toString());
  for (auto &m : ctx->getRealizations()->classes[typeName].members)
    LOG7("       - member: {}: {}", m.first, m.second->toString());
  for (auto &m : ctx->getRealizations()->classes[typeName].methods)
    for (auto &f : m.second)
      LOG7("       - method: {}: {}", m.first, f->toString());
  return typeName;
}

string TransformVisitor::generateTupleStub(int len) {
  auto typeName = fmt::format("Tuple.{}", len);
  if (ctx->getRealizations()->variardicCache.find(typeName) !=
      ctx->getRealizations()->variardicCache.end())
    return typeName;

  ctx->getRealizations()->variardicCache.insert(typeName);
  auto stdlib = ctx->getImports()->getImport("");
  auto oldTC = stdlib->tctx->isTypeChecking();
  stdlib->tctx->typecheck = true;

  vector<Param> generics, args;
  vector<ExprPtr> genericNames;
  for (int i = 1; i <= len; i++) {
    genericNames.push_back(N<IdExpr>(format("T{}", i)));
    generics.push_back(Param{format("T{}", i), nullptr, nullptr});
    args.push_back(Param{format("a{0}", i), N<IdExpr>(format("T{}", i)), nullptr});
  }
  ExprPtr type = N<IdExpr>(typeName);
  if (genericNames.size())
    type = N<IndexExpr>(move(type), N<TupleExpr>(move(genericNames)));
  auto stmt = make_unique<ClassStmt>(true, typeName, move(generics), move(args),
                                     nullptr, vector<string>{});
  stmt->setSrcInfo(ctx->getRealizations()->getGeneratedPos());
  string code = "def __str__(self) -> str:\n";
  code += len ? "  s = '('\n" : "  s = '()'\n";
  for (int i = 0; i < len; i++) {
    code += format("  s += self[{}].__str__()\n", i);
    code += format("  s += '{}'\n", i == len - 1 ? ")" : ", ");
  }
  code += "  return s\n";
  auto fns = parseCode(ctx->getFilename(), code)->getStatements()[0]->clone();
  // LOG7("[VAR] generating {}...\n{}", typeName, code);
  stmt->suite = N<SuiteStmt>(move(fns));

  auto nc = make_shared<TypeContext>(stdlib->tctx->getFilename(),
                                     stdlib->tctx->getRealizations(),
                                     stdlib->tctx->getImports());
  auto newStmt = TransformVisitor(nc).transform(stmt);
  dynamic_cast<SuiteStmt *>(stdlib->statements.get())->stmts.push_back(move(newStmt));
  for (auto &ax : *nc)
    stdlib->tctx->addToplevel(ax.first, ax.second.front());
  stdlib->tctx->typecheck = oldTC;

  return typeName;
}

string TransformVisitor::generatePartialStub(const string &flag) {
  auto typeName = fmt::format("Partial.{}", flag);
  if (ctx->getRealizations()->variardicCache.find(typeName) !=
      ctx->getRealizations()->variardicCache.end())
    return typeName;
  ctx->getRealizations()->variardicCache.insert(typeName);
  auto stdlib = ctx->getImports()->getImport("");
  auto oldTC = stdlib->tctx->isTypeChecking();
  stdlib->tctx->typecheck = true;

  vector<Param> generics, args;
  generics.push_back(Param{"F", nullptr, nullptr});
  args.push_back(Param{"f", N<IdExpr>("F"), nullptr});
  vector<ExprPtr> genericNames;
  genericNames.push_back(N<IdExpr>("F"));
  ExprPtr type = N<IndexExpr>(N<IdExpr>(typeName), N<TupleExpr>(move(genericNames)));
  auto stmt = make_unique<ClassStmt>(true, typeName, move(generics), move(args),
                                     nullptr, vector<string>{});
  stmt->setSrcInfo(ctx->getRealizations()->getGeneratedPos());
  auto nc = make_shared<TypeContext>(stdlib->tctx->getFilename(),
                                     stdlib->tctx->getRealizations(),
                                     stdlib->tctx->getImports());
  auto newStmt = TransformVisitor(nc).transform(stmt);
  dynamic_cast<SuiteStmt *>(stdlib->statements.get())->stmts.push_back(move(newStmt));
  for (auto &ax : *nc)
    stdlib->tctx->addToplevel(ax.first, ax.second.front());
  stdlib->tctx->typecheck = oldTC;

  return typeName;
}

} // namespace ast
} // namespace seq
