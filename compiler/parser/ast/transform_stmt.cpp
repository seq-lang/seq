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
#include "parser/ast/typecontext.h"
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

  DBG("{{ {} ## {}", *stmt, stmt->getSrcInfo().line);
  __level__++;
  stmt->accept(v);
  __level__--;
  if (v.prependStmts->size()) {
    if (v.resultStmt)
      v.prependStmts->push_back(move(v.resultStmt));
    v.resultStmt = N<SuiteStmt>(move(*v.prependStmts));
  }
  DBG("  <> {} }}", v.resultStmt ? v.resultStmt->toString() : "#pass");
  return move(v.resultStmt);
}

PatternPtr TransformVisitor::transform(const Pattern *pat) {
  if (!pat)
    return nullptr;
  TransformVisitor v(ctx, prependStmts);
  pat->accept(v);
  return move(v.resultPattern);
}

void TransformVisitor::visit(const SuiteStmt *stmt) {
  vector<StmtPtr> r;
  for (auto &s : stmt->stmts)
    if (auto t = transform(s))
      r.push_back(move(t));
  resultStmt = N<SuiteStmt>(move(r));
}

void TransformVisitor::visit(const PassStmt *stmt) {
  resultStmt = N<PassStmt>();
}

void TransformVisitor::visit(const BreakStmt *stmt) {
  resultStmt = N<BreakStmt>();
}

void TransformVisitor::visit(const ContinueStmt *stmt) {
  resultStmt = N<ContinueStmt>();
}

void TransformVisitor::visit(const ExprStmt *stmt) {
  resultStmt = N<ExprStmt>(transform(stmt->expr));
}

StmtPtr TransformVisitor::addAssignment(const Expr *lhs, const Expr *rhs,
                                        const Expr *type, bool force) {
  if (auto l = dynamic_cast<const IndexExpr *>(lhs)) {
    vector<ExprPtr> args;
    args.push_back(l->index->clone());
    args.push_back(rhs->clone());
    return transform(Nx<ExprStmt>(
        lhs,
        Nx<CallExpr>(lhs, Nx<DotExpr>(lhs, l->expr->clone(), "__setitem__"),
                     move(args))));
  } else if (auto l = dynamic_cast<const DotExpr *>(lhs)) {
    auto lh = transform(lhs), rh = transform(rhs);
    forceUnify(lh, rh->getType());
    return Nx<AssignStmt>(lhs, move(lh), move(rh));
  } else if (auto l = dynamic_cast<const IdExpr *>(lhs)) {
    auto typExpr = transform(type);
    if (typExpr && !typExpr->isType())
      error(typExpr, "expected a type");
    TypePtr typ = typExpr ? typExpr->getType() : nullptr;
    auto s = Nx<AssignStmt>(lhs, Nx<IdExpr>(l, l->value), transform(rhs, true),
                            move(typExpr), false, force);
    if (typ)
      forceUnify(typ, s->rhs->getType());

    auto t = ctx->find(l->value);
    if (t && !t->isImport()) {
      s->lhs->setType(forceUnify(s->rhs.get(), t->getType()));
    } else {
      if (s->rhs->isType())
        ctx->addType(l->value, s->rhs->getType());
      else if (dynamic_pointer_cast<FuncType>(s->rhs->getType()))
        ctx->addFunc(l->value, s->rhs->getType());
      else
        ctx->add(l->value, s->rhs->getType());
      s->lhs->setType(s->rhs->getType());
    }
    return s;
  } else {
    error(lhs->getSrcInfo(), "invalid assignment");
    return nullptr;
  }
}

void TransformVisitor::processAssignment(const Expr *lhs, const Expr *rhs,
                                         vector<StmtPtr> &stmts, bool force) {
  vector<Expr *> lefts;
  if (auto l = dynamic_cast<const TupleExpr *>(lhs)) {
    for (auto &i : l->items)
      lefts.push_back(i.get());
  } else if (auto l = dynamic_cast<const ListExpr *>(lhs)) {
    for (auto &i : l->items)
      lefts.push_back(i.get());
  } else {
    stmts.push_back(addAssignment(lhs, rhs, nullptr, force));
    return;
  }
  if (!dynamic_cast<const IdExpr *>(rhs)) { // store any non-trivial expression
    auto var = getTemporaryVar("assign");
    auto newRhs = Nx<IdExpr>(rhs, var).release();
    stmts.push_back(addAssignment(newRhs, rhs, nullptr, force));
    rhs = newRhs;
  }
  UnpackExpr *unpack = nullptr;
  int st = 0;
  for (; st < lefts.size(); st++) {
    if (auto u = dynamic_cast<UnpackExpr *>(lefts[st])) {
      unpack = u;
      break;
    }
    processAssignment(
        lefts[st],
        Nx<IndexExpr>(rhs, rhs->clone(), Nx<IntExpr>(rhs, st)).release(), stmts,
        force);
  }
  if (unpack) {
    processAssignment(
        unpack->what.get(),
        Nx<IndexExpr>(
            rhs, rhs->clone(),
            Nx<SliceExpr>(rhs, Nx<IntExpr>(rhs, st),
                          lefts.size() == st + 1
                              ? nullptr
                              : Nx<IntExpr>(rhs, -lefts.size() + st + 1),
                          nullptr))
            .release(),
        stmts, force);
    st += 1;
    for (; st < lefts.size(); st++) {
      if (dynamic_cast<UnpackExpr *>(lefts[st]))
        error(lefts[st]->getSrcInfo(), "two unpack expressions in assignment");
      processAssignment(
          lefts[st],
          Nx<IndexExpr>(rhs, rhs->clone(), Nx<IntExpr>(rhs, -lefts.size() + st))
              .release(),
          stmts, force);
    }
  }
}

// Transformation
void TransformVisitor::visit(const AssignStmt *stmt) {
  vector<StmtPtr> stmts;
  if (stmt->type) {
    if (auto i = CAST(stmt->lhs, IdExpr))
      stmts.push_back(
          addAssignment(stmt->lhs.get(), stmt->rhs.get(), stmt->type.get()));
    else
      error(stmt, "only a single target can be annotated");
  } else {
    processAssignment(stmt->lhs.get(), stmt->rhs.get(), stmts);
  }
  resultStmt = stmts.size() == 1 ? move(stmts[0]) : N<SuiteStmt>(move(stmts));
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
    error(stmt, "this expression cannot be deleted");
  }
}

// Transformation
void TransformVisitor::visit(const PrintStmt *stmt) {
  resultStmt = N<PrintStmt>(conditionalMagic(stmt->expr, "str", "__str__"));
}

void TransformVisitor::visit(const ReturnStmt *stmt) {
  if (stmt->expr) {
    ctx->hasSetReturnType = true;
    auto e = transform(stmt->expr);
    forceUnify(e.get(), ctx->returnType);
    resultStmt = N<ReturnStmt>(move(e));
  } else {
    // if (ctx->returnType->unify(ctx->findInternal("void")) < 0)
    //   error(stmt, "incompatible return types: void and {}",
    //   *ctx->returnType);
    resultStmt = N<ReturnStmt>(nullptr);
  }
}

void TransformVisitor::visit(const YieldStmt *stmt) {
  ctx->hasSetReturnType = true;
  if (stmt->expr) {
    auto e = transform(stmt->expr);
    forceUnify(ctx->returnType,
               ctx->instantiateGeneric(e->getSrcInfo(),
                                       ctx->findInternal("generator"),
                                       {e->getType()}));
    resultStmt = N<YieldStmt>(move(e));
  } else {
    forceUnify(ctx->returnType,
               ctx->instantiateGeneric(stmt->getSrcInfo(),
                                       ctx->findInternal("generator"),
                                       {ctx->findInternal("void")}));
    resultStmt = N<YieldStmt>(nullptr);
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
  auto iter = conditionalMagic(stmt->iter, "generator", "__iter__");
  auto varType = ctx->addUnbound(stmt->var->getSrcInfo());
  if (!iter->getType()->getUnbound()) {
    auto iterType = iter->getType()->getClass();
    if (!iterType || iterType->name != "generator")
      error(iter, "not a generator");
    forceUnify(varType, iterType->explicits[0].type);
  }

  ctx->addBlock();
  if (auto i = CAST(stmt->var, IdExpr)) {
    string varName = i->value;
    ctx->add(varName, varType);
    resultStmt =
        N<ForStmt>(transform(stmt->var), move(iter), transform(stmt->suite));
  } else {
    string varName = getTemporaryVar("for");
    ctx->add(varName, varType);
    vector<StmtPtr> stmts;
    auto var = N<IdExpr>(varName);
    processAssignment(stmt->var.get(), var.get(), stmts, true);
    stmts.push_back(stmt->suite->clone());
    resultStmt = N<ForStmt>(var->clone(), move(iter),
                            transform(N<SuiteStmt>(move(stmts))));
  }
  ctx->popBlock();
}

void TransformVisitor::visit(const IfStmt *stmt) {
  vector<IfStmt::If> ifs;
  for (auto &i : stmt->ifs) {
    auto cond = makeBoolExpr(i.cond);
    ctx->addBlock();
    ifs.push_back({move(cond), transform(i.suite)});
    ctx->popBlock();
  }
  resultStmt = N<IfStmt>(move(ifs));
}

// TODO
void TransformVisitor::visit(const MatchStmt *stmt) {
  error(stmt, "todo match");
  // resultPattern = N<MatchStmt>(transform(stmt->what),
  // transform(stmt->patterns),
  //  transform(stmt->cases));
}

shared_ptr<GenericType>
TransformVisitor::parseGenerics(const vector<Param> &generics) {
  auto genericTypes = make_shared<GenericType>();
  for (auto &g : generics) {
    // if (g.type) {
    //   if (g.type->toString() != "(#id int)")
    //     error(this, "currently only integer static generics are allowed");
    //   genericTypes->explicits.push_back({g.name, StaticType(), });
    //   ctx->addStatic(g.name, 0);
    // } else {
    if (g.type && g.type->toString() != "(#id int)")
      error(this, "currently only integer static generics are allowed");
    genericTypes->explicits.push_back(
        {g.name,
         make_shared<LinkType>(LinkType::Generic,
                               ctx->getRealizations()->getUnboundCount()),
         ctx->getRealizations()->getUnboundCount(), bool(g.type)});
    auto tp = make_shared<LinkType>(LinkType::Unbound,
                                    ctx->getRealizations()->getUnboundCount(),
                                    ctx->level);
    ctx->addType(g.name, tp, false);
    ctx->getRealizations()->getUnboundCount()++;
    // }
  }
  return genericTypes;
}

void TransformVisitor::addMethod(
    Stmt *s, const string &canonicalName,
    const vector<GenericType::Generic> &implicits) {
  if (auto f = dynamic_cast<FunctionStmt *>(s)) {
    transform(f);
    auto name = ctx->getRealizations()->getCanonicalName(f->getSrcInfo());
    auto val = ctx->find(name);
    assert(val);
    auto fv = val->getType()->getFunc();
    assert(fv);
    fv->implicits = implicits;
    ctx->getRealizations()->classes[canonicalName].methods[f->name].push_back(
        fv);
  } else {
    error(s, "types can only contain functions");
  }
}

void TransformVisitor::visit(const ExtendStmt *stmt) {
  TypePtr type;
  vector<string> generics;

  auto getType = [&](const ExprPtr &e) -> TypePtr {
    if (auto i = CAST(e, IdExpr)) {
      auto val = processIdentifier(ctx, i->value);
      if (val->isType())
        return val->getType();
    }
    error("invalid type");
    return nullptr;
  };
  auto genericTypes = make_shared<GenericType>();
  if (auto e = CAST(stmt->what, IndexExpr)) {
    type = getType(e->expr);
    if (auto t = CAST(e->index, TupleExpr))
      for (auto &ti : t->items) {
        if (auto s = CAST(ti, IdExpr))
          generics.push_back(s->value);
        else
          error(ti, "not a valid generic specifier");
      }
    else if (auto i = CAST(e->index, IdExpr))
      generics.push_back(i->value);
    else
      error(e->index, "not a valid generic specifier");
  } else {
    type = getType(stmt->what);
  }
  auto c = type->getClass();
  assert(c);
  auto canonicalName = c->name;
  if (c->explicits.size() != generics.size())
    error(stmt, "generics do not match");
  for (int i = 0; i < generics.size(); i++) {
    auto tp = make_shared<LinkType>(LinkType::Unbound, c->explicits[i].id,
                                    ctx->level);
    ctx->addType(generics[i], tp);
  }
  ctx->increaseLevel();
  ctx->bases.push_back(c->name);
  for (auto s : stmt->suite->getStatements())
    addMethod(s, canonicalName, c->explicits);
  ctx->decreaseLevel();
  for (int i = 0; i < generics.size(); i++) {
    if (c->explicits[i].type) {
      auto t =
          dynamic_pointer_cast<LinkType>(ctx->find(generics[i])->getType());
      assert(t && t->getUnbound());
      t->kind = LinkType::Generic;
    }
    ctx->remove(generics[i]);
  }
  ctx->bases.pop_back();
  resultStmt = nullptr;
}

void TransformVisitor::visit(const ImportStmt *stmt) {
  auto import = ctx->importFile(stmt->from.first);
  if (!import.ctx)
    error(stmt, "cannot locate import '{}'", stmt->from.first);
  if (!stmt->what.size()) {
    ctx->addImport(stmt->from.second == "" ? stmt->from.first
                                           : stmt->from.second,
                   import.filename);
  } else if (stmt->what.size() == 1 && stmt->what[0].first == "*") {
    if (stmt->what[0].second != "")
      error(stmt, "cannot rename star-import");
    for (auto &i : *import.ctx)
      ctx->add(i.first, i.second.top());
  } else {
    for (auto &w : stmt->what) {
      if (auto c = import.ctx->find(w.first))
        ctx->add(w.second == "" ? w.first : w.second, c);
      else
        error(stmt, "symbol '{}' not found in {}", w.first, import.filename);
    }
  }
  resultStmt = stmt->clone();
}

// Transformation
void TransformVisitor::visit(const ExternImportStmt *stmt) {
  if (stmt->lang == "c" && stmt->from) {
    vector<StmtPtr> stmts;
    // ptr = _dlsym(FROM, WHAT)
    stmts.push_back(N<AssignStmt>(
        N<IdExpr>("ptr"), N<CallExpr>(N<IdExpr>("_dlsym"), stmt->from->clone(),
                                      N<StringExpr>(stmt->name.first))));
    // f = function[ARGS](ptr)
    vector<ExprPtr> args;
    args.push_back(stmt->ret ? stmt->ret->clone() : N<IdExpr>("void"));
    for (auto &a : stmt->args)
      args.push_back(a.type->clone());
    stmts.push_back(N<AssignStmt>(
        N<IdExpr>("f"), N<CallExpr>(N<IndexExpr>(N<IdExpr>("function"),
                                                 N<TupleExpr>(move(args))),
                                    N<IdExpr>("ptr"))));
    bool isVoid = true;
    if (stmt->ret) {
      if (auto f = CAST(stmt->ret, IdExpr))
        isVoid = f->value == "void";
      else
        isVoid = false;
    }
    args.clear();
    for (int i = 0; i < stmt->args.size(); i++)
      args.push_back(N<IdExpr>(stmt->args[i].name != "" ? stmt->args[i].name
                                                        : format("$a{}", i)));
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
    resultStmt = transform(N<FunctionStmt>(
        stmt->name.second != "" ? stmt->name.second : stmt->name.first,
        stmt->ret->clone(), vector<Param>(), move(params),
        N<SuiteStmt>(move(stmts)), vector<string>()));
  } else if (stmt->lang == "c") {
    vector<Param> args;
    for (auto &a : stmt->args)
      args.push_back({a.name, transformType(a.type), transform(a.deflt)});
    resultStmt =
        N<ExternImportStmt>(stmt->name, transform(stmt->from),
                            transformType(stmt->ret), move(args), stmt->lang);
  } else if (stmt->lang == "py") {
    vector<StmtPtr> stmts;
    string from = "";
    if (auto i = CAST(stmt->from, IdExpr))
      from = i->value;
    else
      error(stmt, "invalid pyimport query");
    auto call = N<CallExpr>( // _py_import(LIB)[WHAT].call (x.__to_py__)
        N<DotExpr>(N<IndexExpr>(N<CallExpr>(N<IdExpr>("_py_import"),
                                            N<StringExpr>(from)),
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
      stmts.push_back(N<ReturnStmt>(N<CallExpr>(
          N<DotExpr>(stmt->ret->clone(), "__from_py__"), move(call))));
    else
      stmts.push_back(N<ExprStmt>(move(call)));
    vector<Param> params;
    params.push_back({"x", nullptr, nullptr});
    resultStmt = transform(N<FunctionStmt>(
        stmt->name.second != "" ? stmt->name.second : stmt->name.first,
        stmt->ret->clone(), vector<Param>(), move(params),
        N<SuiteStmt>(move(stmts)), vector<string>{"pyhandle"}));
  } else {
    error(stmt, "language {} not yet supported", stmt->lang);
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
      ctx->add(c.var, exc->getType());
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
    error(stmt, "global statements are only applicable within function blocks");
  auto val = ctx->find(stmt->var);
  if (!val || val->isImport() || val->isType())
    error(stmt, "identifier '{}' not found", stmt->var);
  if (val->isVar() && val->getBase() != "")
    error(stmt, "can only mark toplevel variables as global");
  val->setGlobal();
  resultStmt = N<GlobalStmt>(stmt->var);
} // namespace ast

void TransformVisitor::visit(const ThrowStmt *stmt) {
  resultStmt = N<ThrowStmt>(transform(stmt->expr));
}

void TransformVisitor::visit(const FunctionStmt *stmt) {
  auto canonicalName = ctx->getRealizations()->generateCanonicalName(
      stmt->getSrcInfo(), ctx->getModule(),
      format("{}{}", ctx->getBase(), stmt->name));
  if (ctx->getRealizations()->funcASTs.find(canonicalName) ==
      ctx->getRealizations()->funcASTs.end()) {
    vector<TypePtr> argTypes;
    auto genericTypes = parseGenerics(stmt->generics);
    ctx->increaseLevel();
    vector<Param> args;
    vector<FuncType::RealizationInfo::Arg> realizationArgs;
    vector<int> pending;
    argTypes.push_back(stmt->ret ? transformType(stmt->ret)->getType()
                                 : ctx->addUnbound(getSrcInfo(), false));
    for (auto &a : stmt->args) {
      auto t = transformType(a.type);
      argTypes.push_back(
          {a.type ? t->getType() : ctx->addUnbound(getSrcInfo(), false)});
      args.push_back({a.name, move(t)});
      string deflt = "";
      if (a.deflt) {
        deflt = getTemporaryVar(format("def.{}.{}", stmt->name, a.name));
        prepend(N<AssignStmt>(N<IdExpr>(deflt), a.deflt->clone()));
      }
      realizationArgs.push_back({a.name, argTypes.back(), deflt});
      pending.push_back(pending.size());
    }
    ctx->decreaseLevel();

    auto t = make_shared<FuncType>(argTypes, genericTypes);
    t->realizationInfo = make_shared<FuncType::RealizationInfo>(
        canonicalName, pending, realizationArgs);
    t->setSrcInfo(stmt->getSrcInfo());
    t = std::static_pointer_cast<FuncType>(t->generalize(ctx->level));

    DBG("* [function] {} :- {}", canonicalName, *t);
    ctx->addFunc(format("{}{}", ctx->getBase(), stmt->name), t);
    ctx->addFunc(canonicalName, t);
    ctx->getRealizations()->funcASTs[canonicalName] = make_pair(
        t, N<FunctionStmt>(canonicalName, nullptr, CL(stmt->generics),
                           move(args), stmt->suite, stmt->attributes));
  } else {
    ctx->addFunc(format("{}{}", ctx->getBase(), stmt->name),
                 ctx->getRealizations()->funcASTs[canonicalName].first);
    ctx->addFunc(canonicalName,
                 ctx->getRealizations()->funcASTs[canonicalName].first);
  }
  resultStmt = N<FunctionStmt>(stmt->name, nullptr, vector<Param>(),
                               vector<Param>(), nullptr, stmt->attributes);
  resultStmt->setSrcInfo(stmt->getSrcInfo());
}

void TransformVisitor::visit(const ClassStmt *stmt) {
  auto canonicalName = ctx->getRealizations()->generateCanonicalName(
      stmt->getSrcInfo(), ctx->getModule(),
      format("{}{}", ctx->getBase(), stmt->name));
  resultStmt = N<ClassStmt>(stmt->isRecord, stmt->name, vector<Param>(),
                            vector<Param>(), N<SuiteStmt>(), stmt->attributes);

  auto cit = ctx->getRealizations()->classASTs.find(canonicalName);
  if (cit != ctx->getRealizations()->classASTs.end()) {
    ctx->addType(canonicalName, cit->second);
    ctx->addType(format("{}{}", ctx->getBase(), stmt->name), cit->second);
    auto c = ctx->getRealizations()->findClass(canonicalName);
    assert(c);
    for (auto &m : c->methods)
      for (auto &mm : m.second)
        ctx->addFunc(mm->realizationInfo->name, mm);
    return;
  }

  auto genericTypes = parseGenerics(stmt->generics);
  auto ct = make_shared<ClassType>(canonicalName, stmt->isRecord,
                                   vector<TypePtr>(), genericTypes);
  ct->setSrcInfo(stmt->getSrcInfo());
  if (!stmt->isRecord) { // add classes early
    ctx->addType(format("{}{}", ctx->getBase(), stmt->name), ct);
    ctx->addType(canonicalName, ct);
  }
  ctx->getRealizations()->classASTs[canonicalName] = ct;
  DBG("* [class] {} :- {}", canonicalName, *ct);

  ctx->increaseLevel();
  vector<string> strArgs;
  string mainType;
  for (auto &a : stmt->args) {
    assert(a.type);
    auto s = FormatVisitor::format(ctx, a.type);
    strArgs.push_back(format("{}: {}", a.name, s));
    if (!mainType.size())
      mainType = s;
    auto t = transformType(a.type)->getType()->generalize(ctx->level);
    ctx->getRealizations()->classes[canonicalName].members[a.name] = t;
    if (stmt->isRecord)
      ct->recordMembers.push_back(t);
    // DBG("* [class] [member.{}] :- {}", a.name,
    // *ctx->classes[canonicalName].members[a.name]);
  }
  if (!mainType.size())
    mainType = "void";
  if (stmt->isRecord) {
    ctx->addType(format("{}{}", ctx->getBase(), stmt->name), ct);
    ctx->addType(canonicalName, ct);
  }
  ctx->bases.push_back(stmt->name);

  if (std::find(stmt->attributes.begin(), stmt->attributes.end(), "internal") ==
      stmt->attributes.end()) {
    vector<string> genericNames;
    for (auto &g : stmt->generics)
      genericNames.push_back(g.name);
    auto codeType = format("{}{}", stmt->name,
                           genericNames.size()
                               ? format("[{}]", fmt::join(genericNames, ", "))
                               : "");
    string code;
    if (!stmt->isRecord)
      code = format("@internal\ndef __new__() -> {0}: pass\n"
                    "@internal\ndef __bool__(self: {0}) -> bool: pass\n"
                    "@internal\ndef __pickle__(self: {0}, dest: ptr[byte]) -> "
                    "void: pass\n"
                    "@internal\ndef __unpickle__(src: ptr[byte]) -> {0}: pass\n"
                    "@internal\ndef __raw__(self: {0}) -> ptr[byte]: pass\n",
                    codeType);
    else
      code = format(
          "@internal\ndef __new__({1}) -> {0}: pass\n"
          "@internal\ndef __str__(self: {0}) -> str: pass\n"
          "@internal\ndef __getitem__(self: {0}, idx: int) -> {2}: pass\n"
          "@internal\ndef __iter__(self: {0}) -> generator[{2}]: pass\n"
          "@internal\ndef __len__(self: {0}) -> int: pass\n"
          "@internal\ndef __eq__(self: {0}, other: {0}) -> bool: pass\n"
          "@internal\ndef __ne__(self: {0}, other: {0}) -> bool: pass\n"
          "@internal\ndef __lt__(self: {0}, other: {0}) -> bool: pass\n"
          "@internal\ndef __gt__(self: {0}, other: {0}) -> bool: pass\n"
          "@internal\ndef __le__(self: {0}, other: {0}) -> bool: pass\n"
          "@internal\ndef __ge__(self: {0}, other: {0}) -> bool: pass\n"
          "@internal\ndef __hash__(self: {0}) -> int: pass\n"
          "@internal\ndef __contains__(self: {0}, what: {2}) -> bool: pass\n"
          "@internal\ndef __pickle__(self: {0}, dest: ptr[byte]) -> void: "
          "pass\n"
          "@internal\ndef __unpickle__(src: ptr[byte]) -> {0}: pass\n"
          "@internal\ndef __to_py__(self: {0}) -> ptr[byte]: pass\n"
          "@internal\ndef __from_py__(src: ptr[byte]) -> {0}: pass\n",
          codeType, fmt::join(strArgs, ", "), mainType);
    if (!stmt->isRecord && stmt->args.size())
      code += format("@internal\ndef __init__(self: {}, {}) -> void: pass\n",
                     codeType, fmt::join(strArgs, ", "));
    auto methodNew =
        parse_code(ctx->filename, code, stmt->getSrcInfo().line, 100000);
    for (auto s : methodNew->getStatements())
      addMethod(s, canonicalName, genericTypes->explicits);
  }
  for (auto s : stmt->suite->getStatements())
    addMethod(s, canonicalName, genericTypes->explicits);
  ctx->decreaseLevel();
  ctx->bases.pop_back();
  for (auto &g : stmt->generics) {
    // Generalize in place
    auto val = ctx->find(g.name);
    if (val->isType()) {
      auto t = dynamic_pointer_cast<LinkType>(val->getType());
      assert(t && t->kind == LinkType::Unbound);
      t->kind = LinkType::Generic;
    }
    ctx->remove(g.name);
  }
}

// TODO
void TransformVisitor::visit(const DeclareStmt *stmt) {
  error(stmt, "todo declare");
}

// Transformation
void TransformVisitor::visit(const AssignEqStmt *stmt) {
  resultStmt = transform(N<AssignStmt>(
      stmt->lhs->clone(),
      N<BinaryExpr>(stmt->lhs->clone(), stmt->op, stmt->rhs->clone(), true),
      nullptr, true));
}

// Transformation
void TransformVisitor::visit(const YieldFromStmt *stmt) {
  auto var = getTemporaryVar("yield");
  resultStmt = transform(N<ForStmt>(N<IdExpr>(var), stmt->expr->clone(),
                                    N<YieldStmt>(N<IdExpr>(var))));
}

// Transformation
void TransformVisitor::visit(const WithStmt *stmt) {
  if (!stmt->items.size())
    error(stmt, "malformed with statement");
  vector<StmtPtr> content;
  for (int i = stmt->items.size() - 1; i >= 0; i--) {
    vector<StmtPtr> internals;
    string var = stmt->vars[i] == "" ? getTemporaryVar("with") : stmt->vars[i];
    internals.push_back(N<AssignStmt>(N<IdExpr>(var), stmt->items[i]->clone()));
    internals.push_back(
        N<ExprStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "__enter__"))));
    internals.push_back(N<TryStmt>(
        content.size() ? N<SuiteStmt>(move(content)) : stmt->suite->clone(),
        vector<TryStmt::Catch>{},
        N<SuiteStmt>(
            N<ExprStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "__exit__"))))));
    content = move(internals);
  }
  resultStmt =
      transform(N<IfStmt>(N<BoolExpr>(true), N<SuiteStmt>(move(content))));
}

// Transformation
void TransformVisitor::visit(const PyDefStmt *stmt) {
  // _py_exec(""" str """)
  vector<string> args;
  for (auto &a : stmt->args)
    args.push_back(a.name);
  string code = format("def {}({}):\n{}\n", stmt->name, fmt::join(args, ", "),
                       stmt->code);
  resultStmt = transform(N<SuiteStmt>(
      N<ExprStmt>(N<CallExpr>(N<IdExpr>("_py_exec"), N<StringExpr>(code))),
      // from __main__ pyimport foo () -> ret
      N<ExternImportStmt>(make_pair(stmt->name, ""), N<IdExpr>("__main__"),
                          stmt->ret->clone(), vector<Param>(), "py")));
}

void TransformVisitor::visit(const StarPattern *pat) {
  resultPattern = N<StarPattern>();
}

void TransformVisitor::visit(const IntPattern *pat) {
  resultPattern = N<IntPattern>(pat->value);
}

void TransformVisitor::visit(const BoolPattern *pat) {
  resultPattern = N<BoolPattern>(pat->value);
}

void TransformVisitor::visit(const StrPattern *pat) {
  resultPattern = N<StrPattern>(pat->value);
}

void TransformVisitor::visit(const SeqPattern *pat) {
  resultPattern = N<SeqPattern>(pat->value);
}

void TransformVisitor::visit(const RangePattern *pat) {
  resultPattern = N<RangePattern>(pat->start, pat->end);
}

void TransformVisitor::visit(const TuplePattern *pat) {
  resultPattern = N<TuplePattern>(transform(pat->patterns));
}

void TransformVisitor::visit(const ListPattern *pat) {
  resultPattern = N<ListPattern>(transform(pat->patterns));
}

void TransformVisitor::visit(const OrPattern *pat) {
  resultPattern = N<OrPattern>(transform(pat->patterns));
}

void TransformVisitor::visit(const WildcardPattern *pat) {
  resultPattern = N<WildcardPattern>(pat->var);
}

void TransformVisitor::visit(const GuardedPattern *pat) {
  resultPattern =
      N<GuardedPattern>(transform(pat->pattern), transform(pat->cond));
}

void TransformVisitor::visit(const BoundPattern *pat) {
  resultPattern = N<BoundPattern>(pat->var, transform(pat->pattern));
}

/*************************************************************************************/

RealizationContext::FuncRealization TransformVisitor::realize(FuncTypePtr t) {
  assert(t->canRealize());
  auto ret = t->args[0];
  auto name = t->realizationInfo->name;
  auto it = ctx->getRealizations()->funcRealizations.find(name);
  if (it != ctx->getRealizations()->funcRealizations.end()) {
    auto it2 = it->second.find(t->toString(true));
    if (it2 != it->second.end())
      return it2->second;
  } else if (name == "$tuple_str") {
    return {t, nullptr, nullptr}; // already realized
  }

  ctx->addBlock();
  ctx->increaseLevel();
  assert(ctx->getRealizations()->funcASTs.find(name) !=
         ctx->getRealizations()->funcASTs.end());
  auto &ast = ctx->getRealizations()->funcASTs[name];
  ctx->bases.push_back(ast.second->name);
  // Ensure that all inputs are realized
  for (auto &g : t->implicits) {
    if (auto s = g.type->getStatic())
      ctx->addStatic(g.name, s->value);
    else
      ctx->addType(g.name, g.type);
  }
  for (auto &g : t->explicits) {
    if (auto s = g.type->getStatic())
      ctx->addStatic(g.name, s->value);
    else
      ctx->addType(g.name, g.type);
  }
  for (int i = 0; i < t->realizationInfo->args.size(); i++) {
    assert(t->realizationInfo->args[i].type &&
           !t->realizationInfo->args[i].type->hasUnbound());
    ctx->add(ast.second->args[i].name,
             make_shared<LinkType>(t->realizationInfo->args[i].type));
  }
  auto old = ctx->returnType;
  auto oldSeen = ctx->hasSetReturnType;
  ctx->returnType = ret;
  ctx->hasSetReturnType = false;

  // There is no AST linked to internal functions, so just ignore them
  bool isInternal =
      std::find(ast.second->attributes.begin(), ast.second->attributes.end(),
                "internal") != ast.second->attributes.end();
  auto realized = isInternal ? nullptr : realizeBlock(ast.second->suite.get());
  ctx->bases.pop_back();

  // DBG("======== BEGIN {} :- {} ========", t->name, *t);
  if (realized && !ctx->hasSetReturnType && ret)
    forceUnify(ctx->returnType, ctx->findInternal("void"));
  assert(ret->canRealize());
  // DBG("======== END {} :- {} ========", t->name, *t);

  assert(ast.second->args.size() == t->realizationInfo->args.size());
  vector<Param> args;
  for (auto &i : ast.second->args)
    args.push_back({i.name, nullptr, nullptr});
  DBG("<:> {} {}", name, t->toString(true));
  auto result =
      ctx->getRealizations()->funcRealizations[name][t->toString(true)] = {
          t,
          Nx<FunctionStmt>(ast.second.get(), name, nullptr, vector<Param>(),
                           move(args), move(realized), ast.second->attributes),
          nullptr};
  ctx->returnType = old;
  ctx->hasSetReturnType = oldSeen;
  ctx->decreaseLevel();
  ctx->popBlock();
  DBG(">> realized {}::{}", name, *t);
  return result;
}

RealizationContext::ClassRealization TransformVisitor::realize(ClassTypePtr t) {
  assert(t && t->canRealize());
  auto it = ctx->getRealizations()->classRealizations.find(t->name);
  if (it != ctx->getRealizations()->classRealizations.end()) {
    auto it2 = it->second.find(t->toString(true));
    if (it2 != it->second.end())
      return it2->second;
  }

  seq::types::Type *handle = nullptr;
  vector<seq::types::Type *> types;
  vector<int> statics;
  for (auto &m : t->explicits)
    if (auto s = m.type->getStatic())
      statics.push_back(s->value);
    else
      types.push_back(realize(m.type->getClass()).handle);
  // TODO: function ?!
  if (t->name == "Int" || t->name == "UInt") {
    assert(statics.size() == 1 && types.size() == 0);
    if (statics[0] >= 1 && statics[0] <= 2048)
      handle = seq::types::IntNType::get(statics[0], t->name == "Int");
    else
      error("max len is 2018");
  } else if (t->name == "array") {
    assert(types.size() == 1 && statics.size() == 0);
    handle = seq::types::ArrayType::get(types[0]);
  } else if (t->name == "ptr") {
    assert(types.size() == 1 && statics.size() == 0);
    handle = seq::types::PtrType::get(types[0]);
  } else if (t->name == "generator") {
    assert(types.size() == 1 && statics.size() == 0);
    handle = seq::types::GenType::get(types[0]);
  } else if (t->name == "optional") {
    assert(types.size() == 1 && statics.size() == 0);
    handle = seq::types::OptionalType::get(types[0]);
  } else {
    vector<string> names;
    vector<seq::types::Type *> types;
    for (auto &m : ctx->getRealizations()->classes[t->name].members) {
      names.push_back(m.first);
      auto mt = ctx->instantiate(t->getSrcInfo(), m.second, t);
      assert(mt->canRealize() && mt->getClass());
      auto real = realize(mt->getClass());
      types.push_back(real.handle);
    }
    if (t->isRecord())
      handle = seq::types::RecordType::get(types, names,
                                           t->name == "tuple" ? "" : t->name);
    else {
      auto cls = seq::types::RefType::get(t->name);
      cls->setContents(seq::types::RecordType::get(types, names, ""));
      cls->setDone();
      handle = cls;
    }
  }
  DBG(">> realized {}", *t);
  return ctx->getRealizations()
             ->classRealizations[t->name][t->toString(true)] = {t, handle};
}

StmtPtr TransformVisitor::realizeBlock(const Stmt *stmt, bool keepLast,
                                       FILE *fo) {
  if (!stmt)
    return nullptr;
  StmtPtr result = nullptr;

  // We keep running typecheck transformations until there are no more unbound
  // types. It is assumed that the unbound count will decrease in each
  // iteration--- if not, the program cannot be type-checked.
  // TODO: this can be probably optimized one day...
  int reachSize = ctx->activeUnbounds.size();
  for (int iter = 0, prevSize = INT_MAX;; iter++) {
    ctx->addBlock();
    TransformVisitor v(ctx);
    result = v.transform(result ? result.get() : stmt);

    for (auto i = ctx->activeUnbounds.begin();
         i != ctx->activeUnbounds.end();) {
      if (auto l = dynamic_pointer_cast<LinkType>(*i)) {
        if (l->kind != LinkType::Unbound) {
          i = ctx->activeUnbounds.erase(i);
          continue;
        }
      }
      ++i;
    }
    // DBG("post {}", ctx->activeUnbounds.size());
    if (ctx->activeUnbounds.size() >= prevSize) {
      for (auto &ub : ctx->activeUnbounds)
        DBG("NOPE {}", (*ub));
      error("cannot resolve unbound variables");
      break;
    }
    prevSize = ctx->activeUnbounds.size();
    if (prevSize <= reachSize) {
      if (!keepLast)
        ctx->popBlock();
      break;
    } else
      ctx->popBlock();
  }
  if (fo)
    fmt::print(fo, "{}", FormatVisitor::format(ctx, result, true));

  return result;
}

} // namespace ast
} // namespace seq
