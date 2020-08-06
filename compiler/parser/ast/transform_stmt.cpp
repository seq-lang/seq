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
    // LOG9("lhs has type {}, un with {}", mm->toString(), rh->getType()->toString());
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
    resultStmt = N<ExprStmt>(
        transform(N<CallExpr>(N<DotExpr>(N<IdExpr>("_C"), "seq_print"),
                              conditionalMagic(stmt->expr, "str", "__str__"))));
  else
    resultStmt = N<PrintStmt>(transform(stmt->expr));
}

// TODO check if in function!
void TransformVisitor::visit(const ReturnStmt *stmt) {
  if (stmt->expr) {
    ctx->setReturnType();
    auto e = transform(stmt->expr);
    if (ctx->isTypeChecking())
      forceUnify(e.get(), ctx->getReturnType());
    resultStmt = N<ReturnStmt>(move(e));
  } else {
    resultStmt = N<ReturnStmt>(nullptr);
  }
}

void TransformVisitor::visit(const YieldStmt *stmt) {
  ctx->setReturnType();
  if (stmt->expr) {
    auto e = transform(stmt->expr);
    if (ctx->isTypeChecking())
      forceUnify(ctx->getReturnType(),
                 ctx->instantiateGeneric(
                     e->getSrcInfo(), ctx->findInternal("generator"), {e->getType()}));
    resultStmt = N<YieldStmt>(move(e));
  } else {
    if (ctx->isTypeChecking())
      forceUnify(ctx->getReturnType(),
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
  TypePtr varType = nullptr;
  if (ctx->isTypeChecking()) {
    varType = ctx->addUnbound(stmt->var->getSrcInfo());
    if (!iter->getType()->getUnbound()) {
      auto iterType = iter->getType()->getClass();
      if (!iterType || iterType->name != "generator")
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

  for (int i = 0; i < generics.size(); i++) {
    auto l = c->explicits[i].type->getLink();
    assert(l);
    ctx->addType(generics[i],
                 ctx->isTypeChecking()
                     ? make_shared<LinkType>(LinkType::Unbound, c->explicits[i].id,
                                             ctx->getLevel(), nullptr, l->isStatic)
                     : nullptr);
  }
  ctx->increaseLevel();
  ctx->pushBase(c->name);
  ctx->addBaseType(c);
  vector<StmtPtr> funcStmts;
  for (auto s : stmt->suite->getStatements())
    funcStmts.push_back(addMethod(s, canonicalName));

  if (ctx->isTypeChecking())
    for (auto s : stmt->suite->getStatements()) {
      auto stmt = dynamic_cast<FunctionStmt *>(s);
      assert(stmt);
      auto canonicalName = ctx->getRealizations()->generateCanonicalName(
          stmt->getSrcInfo(), format("{}{}", ctx->getBase(), stmt->name));
      auto t = ctx->getRealizations()->funcASTs[canonicalName].first;
      if (t->canRealize()) {
        auto f =
            dynamic_pointer_cast<types::FuncType>(ctx->instantiate(getSrcInfo(), t));
        auto r = realizeFunc(f);
        forceUnify(f, r.type);
      }
    }

  ctx->decreaseLevel();
  for (int i = 0; i < generics.size(); i++) {
    if (ctx->isTypeChecking() && c->explicits[i].type) {
      auto t =
          dynamic_pointer_cast<LinkType>(ctx->find(generics[i])->getType()->follow());
      assert(t && t->kind == LinkType::Unbound);
      t->kind = LinkType::Generic;
    }
    ctx->remove(generics[i]);
  }
  ctx->popBase();
  ctx->popBaseType();
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
        addRelated(c->getType()->getFunc()->canonicalName);
    }
  }

  resultStmt = N<ImportStmt>(make_pair("/" + file, stmt->from.second), stmt->what);
}

// Transformation
void TransformVisitor::visit(const ExternImportStmt *stmt) {
  if (stmt->lang == "c" && stmt->from) {
    vector<StmtPtr> stmts;
    // ptr = _dlsym(FROM, WHAT)
    stmts.push_back(N<AssignStmt>(N<IdExpr>("ptr"),
                                  N<CallExpr>(N<IdExpr>("_dlsym"), stmt->from->clone(),
                                              N<StringExpr>(stmt->name.first))));
    // f = function[ARGS](ptr)
    vector<ExprPtr> args;
    args.push_back(stmt->ret ? stmt->ret->clone() : N<IdExpr>("void"));
    for (auto &a : stmt->args)
      args.push_back(a.type->clone());
    stmts.push_back(N<AssignStmt>(
        N<IdExpr>("f"),
        N<CallExpr>(N<IndexExpr>(N<IdExpr>("function"), N<TupleExpr>(move(args))),
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
        stmt->getSrcInfo(), format("{}{}", ctx->getBase(), stmt->name.first));
    if (!stmt->ret)
      error("expected return type");
    vector<Param> args;
    vector<TypePtr> argTypes{transformType(stmt->ret)->getType()};
    for (auto &a : stmt->args) {
      if (a.deflt)
        error("default arguments not supported here");
      args.push_back({a.name, transformType(a.type), nullptr});
      argTypes.push_back(args.back().type->getType());
    }
    auto t = make_shared<FuncType>(argTypes, vector<Generic>(), nullptr, canonicalName);
    generateVariardicStub("function", argTypes.size());
    t->setSrcInfo(stmt->getSrcInfo());
    t = std::static_pointer_cast<FuncType>(t->generalize(ctx->getLevel()));

    if (!ctx->getBaseType() || ctx->getBaseType()->getFunc()) // class member
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
      stmt->getSrcInfo(), format("{}{}", ctx->getBase(), stmt->name));
  if (ctx->getRealizations()->funcASTs.find(canonicalName) ==
      ctx->getRealizations()->funcASTs.end()) {
    vector<TypePtr> argTypes;
    auto genericTypes = parseGenerics(stmt->generics);
    ctx->increaseLevel();
    vector<Param> args;
    bool isClassMember = ctx->getBaseType() && !ctx->getBaseType()->getFunc();

    // If type checking is not active, make all arguments generic
    auto retTyp = stmt->ret && ctx->isTypeChecking()
                      ? transformType(stmt->ret)->getType()
                      : ctx->addUnbound(getSrcInfo(), false);
    if (isClassMember) { // class member
      // if (stmt->name == "__new__") {
      // forceUnify(retTyp, ctx->instantiate(getSrcInfo(), ctx->getBaseType()));
      // } else
      // if (stmt->name == "__init__") {
      //   forceUnify(retTyp, ctx->findInternal("void"));
      // } else if (stmt->name == "__bool__") {
      //   forceUnify(retTyp, ctx->findInternal("bool"));
      // } else if (stmt->name == "__int__") {
      //   forceUnify(retTyp, ctx->findInternal("int"));
      // } else if (stmt->name == "__str__") {
      //   forceUnify(retTyp, ctx->findInternal("str"));
      // }
    }
    argTypes.push_back(retTyp);

    for (int ia = 0; ia < stmt->args.size(); ia++) {
      auto &a = stmt->args[ia];
      ExprPtr typeAst = nullptr;
      types::TypePtr typ = nullptr;
      if (ctx->isTypeChecking() && ctx->getBaseType() &&
          !ctx->getBaseType()->getFunc() && ia == 0 && !a.type && a.name == "self")
        typ = ctx->getBaseType();
      else if (ctx->isTypeChecking() && a.type) {
        auto ie = CAST(a.type, IndexExpr);
        if (ie && CAST(ie->expr, IdExpr) &&
            CAST(ie->expr, IdExpr)->value == "Callable") {
          vector<TypePtr> args;
          if (auto t = CAST(ie->index, TupleExpr))
            for (auto &i : t->items)
              args.push_back(transformType(i)->getType());
          else
            args.push_back(transformType(ie->index)->getType());
          typ = make_shared<ClassType>("__callable_", true, args);
        } else {
          typeAst = transformType(a.type);
          typ = typeAst->getType();
        }
      } else {
        genericTypes.push_back(
            {"",
             make_shared<LinkType>(LinkType::Generic,
                                   ctx->getRealizations()->getUnboundCount()),
             ctx->getRealizations()->getUnboundCount()});
        typ = ctx->addUnbound(getSrcInfo(), false);
      }
      argTypes.push_back(typ);
      args.push_back({a.name, move(typeAst), a.deflt ? a.deflt->clone() : nullptr});
    }
    ctx->decreaseLevel();
    for (auto &g : stmt->generics) {
      auto val = ctx->find(g.name);
      if (ctx->isTypeChecking())
        if (auto tx = val->getType()) {
          auto t = dynamic_pointer_cast<LinkType>(tx);
          assert(t && t->kind == LinkType::Unbound);
          t->kind = LinkType::Generic;
        }
      ctx->remove(g.name);
    }

    auto parentType = ctx->getBaseType();
    if (ctx->getBaseType() && ctx->getBaseType()->getFunc())
      parentType = nullptr; // only relevant for methods; sub-functions must be
                            // realized in the block
    auto t = make_shared<FuncType>(argTypes, genericTypes, parentType, canonicalName);
    generateVariardicStub("function", argTypes.size());
    t->setSrcInfo(stmt->getSrcInfo());
    t = std::static_pointer_cast<FuncType>(t->generalize(ctx->getLevel()));

    if (!ctx->getBaseType() || ctx->getBaseType()->getFunc())
      ctx->addFunc(stmt->name, t);
    ctx->addGlobal(canonicalName, t);
    LOG9("[stmt] add func {} -> {}", canonicalName, t->toString());
    ctx->getRealizations()->funcASTs[canonicalName] =
        make_pair(t, N<FunctionStmt>(stmt->name, nullptr, CL(stmt->generics),
                                     move(args), stmt->suite, stmt->attributes));

    if (in(stmt->attributes, "builtin")) {
      if (!t->canRealize())
        error("builtins must be realizable");
      auto f = dynamic_pointer_cast<types::FuncType>(ctx->instantiate(getSrcInfo(), t));
      auto r = realizeFunc(f);
      forceUnify(f, r.type);
    } else if (!in(stmt->attributes, "internal")) {
      if (ctx->isTypeChecking() && t->canRealize() && !isClassMember) {
        auto f =
            dynamic_pointer_cast<types::FuncType>(ctx->instantiate(getSrcInfo(), t));
        auto r = realizeFunc(f);
        forceUnify(f, r.type);
      } else {
        ctx->addBlock();
        ctx->increaseLevel();
        ctx->pushBase(stmt->name);
        // Here we just fill the dummy types to generic arguments
        // We just need to do context transformations; types will be
        // filled out later

        for (auto &g : t->explicits)
          if (!g.name.empty())
            ctx->addType(g.name, g.type);
        auto old = ctx->getReturnType();
        auto oldSeen = ctx->wasReturnSet();
        ctx->setReturnType(t->args[0]);
        ctx->setWasReturnSet(false);
        ctx->addBaseType(t);
        for (int i = 1; i < t->args.size(); i++)
          ctx->addVar(stmt->args[i - 1].name, t->args[i]);
        auto oldTC = ctx->isTypeChecking();
        ctx->setTypeCheck(false);

        // __level__++;
        TransformVisitor v(ctx);
        LOG7("=== BEFORE === {} \n{}", canonicalName, stmt->suite->toString());
        auto result = v.transform(stmt->suite.get());
        // TODO : strip types?
        LOG7("=== AFTER ===\n{}", result->toString());
        ctx->getRealizations()->funcASTs[canonicalName].second->suite = move(result);
        // __level__--;
        ctx->popBase();
        ctx->popBaseType();

        ctx->setTypeCheck(oldTC);
        ctx->setReturnType(old);
        ctx->setWasReturnSet(oldSeen);
        ctx->decreaseLevel();
        ctx->popBlock();
      }
    }
  } else {
    if (!ctx->getBaseType() || ctx->getBaseType()->getFunc())
      ctx->addFunc(stmt->name, ctx->getRealizations()->funcASTs[canonicalName].first);
  }
  resultStmt = N<FunctionStmt>(stmt->name, nullptr, vector<Param>(), vector<Param>(),
                               nullptr, stmt->attributes);
}

void TransformVisitor::visit(const ClassStmt *stmt) {
  auto canonicalName = ctx->getRealizations()->generateCanonicalName(
      stmt->getSrcInfo(), format("{}{}", ctx->getBase(), stmt->name));
  resultStmt = N<ClassStmt>(stmt->isRecord, stmt->name, vector<Param>(),
                            vector<Param>(), N<SuiteStmt>(), stmt->attributes);
  auto cit = ctx->getRealizations()->classASTs.find(canonicalName);
  if (cit != ctx->getRealizations()->classASTs.end()) {
    ctx->addType(stmt->name, cit->second);
    return;
  }

  vector<StmtPtr> stmts;
  stmts.push_back(move(resultStmt));

  // ClassTypePtr ct = nullptr;
  // if (chop(canonicalName).substr(0, 11) == "__function_")
  // ct = make_shared<FuncType>(vector<TypePtr>(), genericTypes,
  // ctx->getBaseType());
  // else
  auto ct = make_shared<ClassType>(canonicalName, stmt->isRecord, vector<TypePtr>(),
                                   parseGenerics(stmt->generics), ctx->getBaseType());
  ct->setSrcInfo(stmt->getSrcInfo());
  if (!stmt->isRecord) { // add classes early
    ctx->addType(stmt->name, ct);
    ctx->addGlobal(canonicalName, ct);
  }
  ctx->getRealizations()->classASTs[canonicalName] = ct;
  LOG9("[stmt] add class {} -> {}", canonicalName, ct->toString());

  ctx->increaseLevel();
  unordered_set<string> seenMembers;
  ExprPtr mainType = nullptr;
  for (auto &a : stmt->args) {
    assert(a.type);
    if (!mainType)
      mainType = a.type->clone();
    auto t = transformType(a.type)->getType()->generalize(ctx->getLevel());
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
    ctx->addType(stmt->name, ct);
    ctx->addGlobal(canonicalName, ct);
  }
  ctx->pushBase(stmt->name);
  ctx->addBaseType(ct);
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
          "__iter__", N<IndexExpr>(N<IdExpr>("generator"), N<IdExpr>("int")),
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

  if (ctx->isTypeChecking())
    for (int i = 1; i < stmts.size(); i++) {
      auto stmt = dynamic_cast<FunctionStmt *>(stmts[i].get());
      assert(stmt);
      auto canonicalName = ctx->getRealizations()->generateCanonicalName(
          stmt->getSrcInfo(), format("{}{}", ctx->getBase(), stmt->name));
      auto t = ctx->getRealizations()->funcASTs[canonicalName].first;
      if (t->canRealize()) {
        auto f =
            dynamic_pointer_cast<types::FuncType>(ctx->instantiate(getSrcInfo(), t));
        auto r = realizeFunc(f);
        forceUnify(f, r.type);
      }
    }

  ctx->decreaseLevel();
  ctx->popBase();
  ctx->popBaseType();
  for (auto &g : stmt->generics) {
    // Generalize in place
    auto val = ctx->find(g.name);
    if (auto tx = val->getType()) {
      auto t = dynamic_pointer_cast<LinkType>(tx);
      assert(t && t->kind == LinkType::Unbound);
      t->kind = LinkType::Generic;
    }
    ctx->remove(g.name);
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
