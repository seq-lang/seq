/*
 * typecheck_expr.cpp --- Type inference for AST expressions.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "parser/ast.h"
#include "parser/common.h"
#include "parser/visitors/simplify/simplify.h"
#include "parser/visitors/typecheck/typecheck.h"

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

ExprPtr TypecheckVisitor::transform(const ExprPtr &expr) {
  return transform(expr, false);
}

ExprPtr TypecheckVisitor::transform(const ExprPtr &expr, bool allowTypes) {
  if (!expr)
    return nullptr;
  TypecheckVisitor v(ctx, prependStmts);
  v.setSrcInfo(expr->getSrcInfo());
  expr->accept(v);
  if (v.resultExpr && v.resultExpr->getType() && v.resultExpr->getType()->getClass() &&
      v.resultExpr->getType()->getClass()->canRealize())
    realizeType(v.resultExpr->getType()->getClass());
  seqassert(v.resultExpr, "cannot parse {}", expr->toString());
  return move(v.resultExpr);
}

ExprPtr TypecheckVisitor::transformType(const ExprPtr &expr) {
  auto e = transform(expr, true);
  if (e) {
    if (!e->isType())
      error("expected type expression");
    e->setType(ctx->instantiate(expr->getSrcInfo(), e->getType()));
  }
  return e;
}

void TypecheckVisitor::defaultVisit(const Expr *e) { resultExpr = e->clone(); }

void TypecheckVisitor::visit(const BoolExpr *expr) {
  resultExpr = expr->clone();
  resultExpr->setType(ctx->findInternal("bool"));
}

void TypecheckVisitor::visit(const IntExpr *expr) {
  resultExpr = expr->clone();
  resultExpr->setType(ctx->findInternal("int"));
}

void TypecheckVisitor::visit(const FloatExpr *expr) {
  resultExpr = expr->clone();
  resultExpr->setType(ctx->findInternal("float"));
}

void TypecheckVisitor::visit(const StringExpr *expr) {
  resultExpr = expr->clone();
  resultExpr->setType(ctx->findInternal("str"));
}

void TypecheckVisitor::visit(const IdExpr *expr) {
  auto val = ctx->find(expr->value);
  if (!val)
    ctx->dump();
  seqassert(val, "cannot find '{}'", expr->value);
  if (expr->value == "Generator")
    assert(1);
  if (val->isStatic()) {
    auto s = val->type->getStatic();
    assert(s);
    resultExpr = transform(N<IntExpr>(s->getValue()));
  } else {
    resultExpr = expr->clone();
    TypePtr typ = val->type;
    if (val->isType())
      resultExpr->markType();
    typ = ctx->instantiate(getSrcInfo(), val->type);
    resultExpr->setType(forceUnify(resultExpr, typ));
    auto newName = patchIfRealizable(typ, val->isType());
    if (!newName.empty())
      static_cast<IdExpr *>(resultExpr.get())->value = newName;
  }
}

void TypecheckVisitor::visit(const IfExpr *expr) {
  auto e = N<IfExpr>(transform(expr->cond), transform(expr->ifexpr),
                     transform(expr->elsexpr));
  auto ti = e->ifexpr->getType()->getClass();
  auto te = e->elsexpr->getType()->getClass();
  if (ti && te) {
    if (te->name != ti->name) {
      if (ti->name == "Optional")
        e->elsexpr = transform(N<CallExpr>(N<IdExpr>("Optional"), move(e->elsexpr)));
      else if (te->name == "Optional")
        e->ifexpr = transform(N<CallExpr>(N<IdExpr>("Optional"), move(e->ifexpr)));
    }
    forceUnify(e->ifexpr->getType(), e->elsexpr->getType());
  }
  e->setType(forceUnify(expr, e->ifexpr->getType()));
  resultExpr = move(e);
}

void TypecheckVisitor::visit(const BinaryExpr *expr) {
  auto e = transformBinary(expr->lexpr, expr->rexpr, expr->op, expr->inPlace);
  e->setType(forceUnify(expr, e->getType()));
  resultExpr = move(e);
}

void TypecheckVisitor::visit(const PipeExpr *expr) {
  auto extractType = [&](TypePtr t) {
    auto c = t->getClass();
    if (c && c->name == "Generator")
      return c->explicits[0].type;
    else
      return t;
  };

  vector<PipeExpr::Pipe> items;
  items.push_back({expr->items[0].op, transform(expr->items[0].expr)});
  vector<types::TypePtr> types;
  TypePtr inType = items.back().expr->getType();
  types.push_back(inType);
  inType = extractType(inType);
  int inTypePos = 0;
  for (int i = 1; i < expr->items.size(); i++) {
    auto l = expr->items[i].clone();

  reset:
    if (auto ce = CAST(l.expr, CallExpr)) {
      // TODO: what if this is a StmtExpr [e.g. a constructor]?
      int inTypePos = -1;
      for (int ia = 0; ia < ce->args.size(); ia++)
        if (auto ee = CAST(ce->args[ia].value, EllipsisExpr)) {
          if (inTypePos == -1)
            ee->isPipeArg = true, inTypePos = ia;
          else
            error(ce->args[ia].value, "unexpected partial argument");
        }
      if (inTypePos == -1) {
        ce->args.insert(ce->args.begin(), {"", N<EllipsisExpr>(true)});
        inTypePos = 0;
      }

      // forceUnify(ce->args[inTypePos].value, inType);
      ExprPtr st = nullptr;
      auto n = parseCall(ce, inType, &st);
      if (st) {
        l = {"|>", move(st)};
        i--;
        goto reset;
      }
      items.push_back({l.op, move(n)});
    } else {
      auto c = N<CallExpr>(clone(l.expr), N<EllipsisExpr>(true));
      // forceUnify(c->args[0].value, inType);

      ExprPtr st = nullptr;
      auto n = parseCall(c.get(), inType, &st);
      if (st) {
        l = {"|>", move(st)};
        i--;
        goto reset;
      }
      items.push_back({l.op, move(n)});
      inTypePos = 0;
    }
    inType = items.back().expr->getType();
    types.push_back(inType);

    if (i < expr->items.size() - 1)
      inType = extractType(inType);
  }
  resultExpr = N<PipeExpr>(move(items));
  CAST(resultExpr, PipeExpr)->inTypes = types;
  resultExpr->setType(forceUnify(expr, inType));
}

void TypecheckVisitor::visit(const StaticExpr *expr) {
  // when visited "normally" just treat it as normal expression
  resultExpr = transform(expr->expr);
}

void TypecheckVisitor::visit(const InstantiateExpr *expr) {
  ExprPtr e = transform(expr->typeExpr, true);
  auto g = ctx->instantiate(e->getSrcInfo(), e->getType());
  for (int i = 0; i < expr->typeParams.size(); i++) {
    TypePtr t = nullptr;
    if (auto s = CAST(expr->typeParams[i], StaticExpr)) {
      map<string, Generic> m;
      for (auto g : s->captures) {
        auto val = ctx->find(g);
        assert(val && val->isStatic());
        auto t = val->type->follow();
        m[g] = {g, t,
                t->getLink()                       ? t->getLink()->id
                : t->getStatic()->explicits.size() ? t->getStatic()->explicits[0].id
                                                   : 0};
      }
      auto sv = StaticVisitor(m);
      sv.transform(s->expr);
      if (auto ie = CAST(s->expr, IdExpr)) { /// special case: generic static expr
        assert(m.size() == 1);
        t = ctx->instantiate(getSrcInfo(), m.begin()->second.type);
      } else {
        vector<Generic> v;
        for (auto &i : m)
          v.push_back(i.second);
        t = make_shared<StaticType>(v, clone(s->expr));
      }
    } else {
      if (!expr->typeParams[i]->isType())
        error(expr->typeParams[i], "not a type");
      t = ctx->instantiate(getSrcInfo(), transformType(expr->typeParams[i])->getType());
    }
    /// Note: at this point, only single-variable static var expression (e.g.
    /// N) is allowed, so unify will work as expected.
    if (g->getFunc()) {
      if (i >= g->getFunc()->explicits.size())
        error("expected {} generics", g->getFunc()->explicits.size());
      forceUnify(g->getFunc()->explicits[i].type, t);
    } else {
      if (i >= g->getClass()->explicits.size())
        error("expected {} generics", g->getClass()->explicits.size());
      forceUnify(g->getClass()->explicits[i].type, t);
    }
  }
  bool isType = e->isType();
  auto t = forceUnify(expr, g);
  auto newName = patchIfRealizable(t, isType);
  auto i = CAST(e, IdExpr);
  if (!newName.empty() && i && newName != i->value) {
    auto comp = split(newName, ':');
    if (startswith(comp.back(), i->value))
      i->value = newName;
  }
  resultExpr = move(e); // will get replaced by identifier later on
  if (isType)
    resultExpr->markType();
  resultExpr->setType(t);
}

void TypecheckVisitor::visit(const SliceExpr *expr) {
  ExprPtr none = N<CallExpr>(N<DotExpr>(N<IdExpr>("Optional"), "__new__"));
  resultExpr = transform(N<CallExpr>(N<IdExpr>("Slice"),
                                     expr->start ? clone(expr->start) : transform(none),
                                     expr->stop ? clone(expr->stop) : transform(none),
                                     expr->step ? clone(expr->step) : transform(none)));
}

void TypecheckVisitor::visit(const IndexExpr *expr) {
  auto getTupleIndex = [&](ClassType *tuple, const auto &expr,
                           const auto &index) -> ExprPtr {
    if (!tuple->isRecord())
      return nullptr;
    if (tuple->name == "Ptr" || tuple->name == "Array" || tuple->name == "Optional")
      return nullptr;
    if (!startswith(tuple->name, "Tuple.N")) { // avoid if there is a __getitem__ here
      // auto m = ctx->findMethod(tuple->name, "__getitem__");
      // if (m && m->size() > 1)
      return nullptr;
      // TODO : be smarter! there might be a compatible getitem?
    }
    auto mm = ctx->cache->classes.find(tuple->name);
    assert(mm != ctx->cache->classes.end());
    auto getInt = [](seq_int_t *o, const ExprPtr &e) {
      if (!e)
        return true;
      if (auto i = CAST(e, IntExpr)) {
        *o = i->intValue;
        return true;
      }
      return false;
    };
    seq_int_t s = 0, e = tuple->args.size(), st = 1;
    if (auto ex = CAST(index, IntExpr)) {
      int i = translateIndex(ex->intValue, e);
      if (i < 0 || i >= e)
        error("tuple index out of range (expected 0..{}, got {})", e, i);
      return transform(N<DotExpr>(clone(expr), mm->second.fields[i].name));
    } else if (auto sx = CAST(index, StaticExpr)) {
      map<string, types::Generic> m;
      auto sv = StaticVisitor(m);
      auto r = sv.transform(sx->expr);
      assert(r.first);
      int i = translateIndex(r.second, e);
      if (i < 0 || i >= e)
        error("tuple index out of range (expected 0..{}, got {})", e, i);
      return transform(N<DotExpr>(clone(expr), mm->second.fields[i].name));
    } else if (auto i = CAST(index, SliceExpr)) {
      if (!getInt(&s, i->start) || !getInt(&e, i->stop) || !getInt(&st, i->step))
        return nullptr;
      if (i->step && !i->start)
        s = st > 0 ? 0 : tuple->args.size();
      if (i->step && !i->stop)
        e = st > 0 ? tuple->args.size() : 0;
      sliceAdjustIndices(tuple->args.size(), &s, &e, st);
      vector<ExprPtr> te;
      for (auto i = s; (st >= 0) ? (i < e) : (i >= e); i += st) {
        if (i < 0 || i >= tuple->args.size())
          error("tuple index out of range (expected 0..{}, got {})", tuple->args.size(),
                i);
        te.push_back(N<DotExpr>(clone(expr), mm->second.fields[i].name));
      }
      return transform(N<CallExpr>(
          N<DotExpr>(N<IdExpr>(format("Tuple.N{}", te.size())), "__new__"), move(te)));
    }
    return nullptr;
  };

  ExprPtr e = transform(expr->expr, true);
  auto t = e->getType();
  if (t->getFunc()) {
    vector<ExprPtr> it;
    if (auto t = CAST(expr->index, TupleExpr))
      for (auto &i : t->items)
        it.push_back(clone(i));
    else
      it.push_back(clone(expr->index));
    resultExpr = transform(N<InstantiateExpr>(move(e), move(it)));
  } else if (auto c = t->getClass()) {
    resultExpr = getTupleIndex(c.get(), expr->expr, expr->index);
    if (!resultExpr)
      resultExpr = transform(N<CallExpr>(N<DotExpr>(expr->expr->clone(), "__getitem__"),
                                         expr->index->clone()));
  } else {
    resultExpr = N<IndexExpr>(move(e), transform(expr->index));
    resultExpr->setType(
        forceUnify(expr, ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel)));
  }
}

void TypecheckVisitor::visit(const StackAllocExpr *expr) {
  auto te = transformType(expr->typeExpr);
  auto e = transform(expr->expr);

  auto t = te->getType();
  resultExpr = N<StackAllocExpr>(move(te), move(e));
  t = ctx->instantiateGeneric(expr->getSrcInfo(), ctx->findInternal("Array"), {t});
  patchIfRealizable(t, true);
  resultExpr->setType(forceUnify(expr, t));
}

void TypecheckVisitor::visit(const CallExpr *expr) { resultExpr = parseCall(expr); }

void TypecheckVisitor::visit(const DotExpr *expr) {
  resultExpr = visitDot(expr);
  forceUnify(expr, resultExpr->getType());
}

void TypecheckVisitor::visit(const EllipsisExpr *expr) {
  resultExpr = N<EllipsisExpr>(expr->isPipeArg);
  resultExpr->setType(ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel));
}

void TypecheckVisitor::visit(const TypeOfExpr *expr) {
  auto e = transform(expr->expr);
  auto t = forceUnify(expr, e->getType());

  auto newName = patchIfRealizable(t, true);
  if (!newName.empty())
    resultExpr = N<IdExpr>(newName);
  else
    resultExpr = N<TypeOfExpr>(move(e));
  resultExpr->markType();
  resultExpr->setType(t);
}

void TypecheckVisitor::visit(const PtrExpr *expr) {
  auto param = transform(expr->expr);
  auto t = param->getType();
  resultExpr = N<PtrExpr>(move(param));
  resultExpr->setType(
      forceUnify(expr, ctx->instantiateGeneric(expr->getSrcInfo(),
                                               ctx->findInternal("Ptr"), {t})));
}

void TypecheckVisitor::visit(const YieldExpr *expr) {
  resultExpr = N<YieldExpr>();
  if (ctx->bases.size() <= 1)
    error("(yield) cannot be used outside of functions");
  auto t =
      ctx->instantiateGeneric(getSrcInfo(), ctx->findInternal("Generator"),
                              {ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel)});
  auto &base = ctx->bases.back();
  if (base.returnType)
    t = forceUnify(base.returnType, t);
  else
    base.returnType = t;
  auto c = t->follow()->getClass();
  assert(c);
  resultExpr->setType(forceUnify(expr, c->explicits[0].type));
}

void TypecheckVisitor::visit(const StmtExpr *expr) {
  vector<StmtPtr> stmts;
  for (auto &s : expr->stmts)
    stmts.push_back(transform(s));
  auto e = transform(expr->expr);
  auto t = forceUnify(expr, e->getType());
  resultExpr = N<StmtExpr>(move(stmts), move(e));
  resultExpr->setType(t);
}

ExprPtr TypecheckVisitor::transformBinary(const ExprPtr &lexpr, const ExprPtr &rexpr,
                                          const string &op, bool inPlace, bool isAtomic,
                                          bool *noReturn) {
  auto magics = unordered_map<string, string>{
      {"+", "add"},     {"-", "sub"},    {"*", "mul"}, {"**", "pow"}, {"/", "truediv"},
      {"//", "div"},    {"@", "matmul"}, {"%", "mod"}, {"<", "lt"},   {"<=", "le"},
      {">", "gt"},      {">=", "ge"},    {"==", "eq"}, {"!=", "ne"},  {"<<", "lshift"},
      {">>", "rshift"}, {"&", "and"},    {"|", "or"},  {"^", "xor"},  {"min", "min"},
      {"max", "max"}};
  if (noReturn)
    *noReturn = false;
  auto le = transform(lexpr);
  auto re = CAST(rexpr, NoneExpr) ? clone(rexpr) : transform(rexpr);
  if (le->getType()->getUnbound() || (op != "is" && re->getType()->getUnbound())) {
    auto e = N<BinaryExpr>(move(le), op, move(re));
    e->setType(ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel));
    return e;
  } else if (op == "&&" || op == "||") {
    auto e = N<BinaryExpr>(move(le), op, move(re));
    e->setType(ctx->findInternal("bool"));
    return e;
  } else if (op == "is") {
    if (CAST(rexpr, NoneExpr)) {
      if (le->getType()->getClass()->name != "Optional")
        return transform(N<BoolExpr>(false));
      else
        return transform(N<CallExpr>(
            N<DotExpr>(N<CallExpr>(N<DotExpr>(move(le), "__bool__")), "__invert__")));
    }
    ExprPtr e;
    if (!le->getType()->canRealize() || !re->getType()->canRealize()) {
      e = N<BinaryExpr>(move(le), op, move(re));
    } else {
      auto lc = realizeType(le->getType()->getClass());
      auto rc = realizeType(re->getType()->getClass());
      if (!lc || !rc)
        error("both sides of 'is' expression must be of same reference type");
      e = transform(N<BinaryExpr>(N<CallExpr>(N<DotExpr>(move(le), "__raw__")),
                                  "==", N<CallExpr>(N<DotExpr>(move(re), "__raw__"))));
    }
    e->setType(ctx->findInternal("bool"));
    return e;
  } else {
    auto mi = magics.find(op);
    if (mi == magics.end())
      error("invalid binary operator '{}'", op);
    auto magic = mi->second;
    auto lc = le->getType()->getClass(), rc = re->getType()->getClass();
    assert(lc && rc);
    auto plc = ctx->instantiateGeneric(getSrcInfo(), ctx->findInternal("Ptr"), {lc});
    FuncTypePtr f;
    if (isAtomic &&
        (f = findBestCall(lc, format("__atomic_{}__", magic), {{"", plc}, {"", rc}}))) {
      le = N<PtrExpr>(move(le));
      if (noReturn)
        *noReturn = true;
    } else if (inPlace &&
               (f = findBestCall(lc, format("__i{}__", magic), {{"", lc}, {"", rc}}))) {
      if (noReturn)
        *noReturn = true;
    } else if ((f = findBestCall(lc, format("__{}__", magic), {{"", lc}, {"", rc}}))) {
      ;
    } else if ((f = findBestCall(rc, format("__r{}__", magic), {{"", rc}, {"", lc}}))) {
      ;
    } else {
      error("cannot find magic '{}' for {}", magic, lc->toString());
    }
    return transform(N<CallExpr>(N<IdExpr>(f->name), move(le), move(re)));
  }
}

ExprPtr TypecheckVisitor::visitDot(const DotExpr *expr, vector<CallExpr::Arg> *args) {
  auto isMethod = [&](FuncTypePtr f) {
    auto ast = ctx->cache->functions[f->name].ast.get();
    return in(ast->attributes, ATTR_NOT_STATIC);
  };
  auto deactivateUnbounds = [&](Type *t) {
    auto ub = t->getUnbounds();
    for (auto &u : ub)
      ctx->activeUnbounds.erase(u);
  };

  auto lhs = transform(expr->expr, true);
  TypePtr typ = nullptr;
  if (lhs->getType()->getUnbound()) {
    typ = ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel);
  } else if (auto c = lhs->getType()->getClass()) {
    auto m = ctx->findMethod(c->name, expr->member);
    if (!m.empty()) {
      if (args) {
        vector<pair<string, TypePtr>> targs;
        if (!lhs->isType())
          targs.emplace_back(make_pair("", c));
        for (auto &a : *args)
          targs.emplace_back(make_pair(a.name, a.value->getType()));
        if (auto m = findBestCall(c, expr->member, targs, true)) {
          if (!lhs->isType())
            args->insert(args->begin(), {"", clone(lhs)});
          auto e = N<IdExpr>(m->name);
          e->setType(ctx->instantiate(getSrcInfo(), m, c.get()));
          if (lhs->isType() && !isMethod(m))
            deactivateUnbounds(c.get());
          return e;
        } else {
          vector<string> nice;
          for (auto &t : targs)
            nice.emplace_back(format("{} = {}", t.first, t.second->toString()));
          error("cannot find method '{}' in {} with arguments {}", expr->member,
                c->toString(), join(nice, ", "));
        }
      }

      FuncTypePtr bestCall = nullptr;
      if (m.size() > 1) {
        // need to check is this a callable that we can use to instantiate the type
        if (expr->getType() && expr->getType()->getClass()) {
          auto dc = expr->getType()->getClass();
          if (startswith(dc->name, "Function.N")) {
            vector<pair<string, TypePtr>> targs; // we can, well, unify this
            if (!lhs->isType())
              targs.emplace_back(make_pair("", c));
            for (auto i = 1; i < dc->explicits.size(); i++)
              targs.emplace_back(make_pair("", dc->explicits[i].type));
            if (auto mc = findBestCall(c, expr->member, targs, true))
              bestCall = mc;
            else {
              vector<string> nice;
              for (auto &t : targs)
                nice.emplace_back(format("{} = {}", t.first, t.second->toString()));
              error("cannot find method '{}' in {} with arguments {}", expr->member,
                    c->toString(), join(nice, ", "));
            }
          }
        }
      } else {
        bestCall = m[0];
      }
      if (!bestCall) {
        // TODO: fix this and have better method for handling these cases
        bestCall = m[0];
      }
      if (lhs->isType()) {
        auto name = bestCall->name;
        auto val = ctx->find(name);
        assert(val);
        auto t = ctx->instantiate(getSrcInfo(), bestCall, c.get());
        auto e = N<IdExpr>(name);
        e->setType(t);
        auto newName = patchIfRealizable(t, val->isType());
        if (!newName.empty())
          e->value = newName;
        if (!isMethod(bestCall))
          deactivateUnbounds(c.get());
        return e;
      } else { // cast y.foo to CLS.foo(y, ...)
        auto f = bestCall;
        vector<ExprPtr> args;
        args.push_back(move(lhs));
        for (int i = 0; i < std::max(1, (int)f->args.size() - 2); i++)
          args.push_back(N<EllipsisExpr>());
        auto ast = ctx->cache->functions[f->name].ast.get();
        if (in(ast->attributes, "property"))
          args.pop_back();
        return transform(N<CallExpr>(N<IdExpr>(bestCall->name), move(args)));
      }
    } else if (auto mm = ctx->findMember(c->name, expr->member)) {
      typ = ctx->instantiate(getSrcInfo(), mm, c.get());
    } else if (c->name == "Optional") {
      auto d = N<DotExpr>(
          transform(N<CallExpr>(N<IdExpr>("unwrap"), clone(expr->expr))), expr->member);
      return visitDot(d.get(), args);
    } else if (c->name == "pyobj") {
      return transform(N<CallExpr>(N<DotExpr>(clone(expr->expr), "_getattr"),
                                   N<StringExpr>(expr->member)));
    } else {
      error("cannot find '{}' in {}", expr->member, lhs->getType()->toString());
    }
  } else {
    error("cannot find '{}' in {}", expr->member, lhs->getType()->toString());
  }
  auto t = N<DotExpr>(move(lhs), expr->member);
  t->setType(typ);
  return t;
}

ExprPtr TypecheckVisitor::parseCall(const CallExpr *expr, types::TypePtr inType,
                                    ExprPtr *extraStage) {
  vector<CallExpr::Arg> args;
  for (auto &i : expr->args) {
    args.push_back({i.name, transform(i.value)});
    if (auto e = CAST(i.value, EllipsisExpr)) {
      if (inType && e->isPipeArg &&
          !inType->getUnbound()) // if unbound, might be a generator and unpack later
        forceUnify(inType, args.back().value->getType());
      else
        forceUnify(i.value, args.back().value->getType());
    }
  }

  ExprPtr callee = nullptr;
  Expr *lhs = const_cast<CallExpr *>(expr)->expr.get();
  if (auto i = CAST(expr->expr, IndexExpr))
    lhs = i->expr.get();
  else if (auto i = CAST(expr->expr, InstantiateExpr))
    lhs = i->typeExpr.get();
  if (auto i = dynamic_cast<DotExpr *>(lhs)) {
    callee = visitDot(i, &args);
    if (auto i = CAST(expr->expr, IndexExpr))
      callee = transform(N<IndexExpr>(move(callee), clone(i->index)));
    else if (auto i = CAST(expr->expr, InstantiateExpr))
      callee = transform(N<InstantiateExpr>(move(callee), clone(i->typeParams)));
  } else {
    callee = transform(expr->expr, true);
  }
  forceUnify(expr->expr.get(), callee->getType());

  auto calleeType = callee->getType();
  auto calleeClass = callee->getType()->getClass();
  if (!calleeClass) { // Unbound caller, will be handled later
    callee = N<CallExpr>(move(callee), move(args));
    callee->setType(
        forceUnify(expr, ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel)));
    return callee;
  } else if (callee->isType() && calleeClass->isRecord()) {
    return transform(N<CallExpr>(N<DotExpr>(move(callee), "__new__"), move(args)));
  } else if (callee->isType()) {
    /// WARN: passing callee & args that have already been transformed
    ExprPtr var = N<IdExpr>(ctx->cache->getTemporaryVar("v"));
    vector<StmtPtr> stmts;
    stmts.push_back(
        N<AssignStmt>(clone(var), N<CallExpr>(N<DotExpr>(move(callee), "__new__"))));
    stmts.push_back(
        N<ExprStmt>(N<CallExpr>(N<DotExpr>(clone(var), "__init__"), move(args))));
    return transform(N<StmtExpr>(move(stmts), clone(var)));
  } else if (!calleeClass->getCallable()) {
    if (calleeClass->name == "pyobj") {
      if (args.size() != 1 ||
          !(args[0].value->getType()->getClass() &&
            startswith(args[0].value->getType()->getClass()->name, "Tuple.N"))) {
        vector<ExprPtr> e;
        for (auto &a : args) {
          if (a.name != "")
            error("named python calls are not yet supported");
          e.push_back(move(a.value));
        }
        auto ne = transform(N<CallExpr>(
            N<DotExpr>(N<IdExpr>(format("Tuple.N{}", args.size())), "__new__"),
            move(e)));
        args.clear();
        args.push_back({"", move(ne)});
      }
    }
    return transform(N<CallExpr>(N<DotExpr>(move(callee), "__call__"), move(args)));
  }

  FunctionStmt *ast = nullptr;
  if (auto ff = calleeType->getFunc()) {
    ast = ctx->cache->functions[ff->name].ast.get();
  }

  // Handle named and default arguments
  vector<CallExpr::Arg> reorderedArgs;
  vector<int> argIndex;
  string knownTypes;
  if (startswith(calleeClass->name, "Partial.N")) {
    knownTypes = calleeClass->name.substr(9);
    calleeType = calleeClass->args[0];
    calleeClass = calleeClass->args[0]->getClass();
    assert(calleeClass);
  }
  for (int i = 0; i < int(calleeClass->args.size()) - 1; i++)
    if (knownTypes.empty() || knownTypes[i] == '0')
      argIndex.push_back(i);

  vector<int> pending;
  bool isPartial = false;
  bool namesStarted = false;
  unordered_map<string, ExprPtr> namedArgs;
  for (int i = 0; i < args.size(); i++) {
    if (args[i].name == "" && namesStarted)
      error("unnamed argument after a named argument");
    namesStarted |= args[i].name != "";
    if (args[i].name == "")
      reorderedArgs.push_back({"", move(args[i].value)});
    else if (namedArgs.find(args[i].name) == namedArgs.end())
      namedArgs[args[i].name] = move(args[i].value);
    else
      error("named argument {} repeated multiple times", args[i].name);
  }

  if (namedArgs.size() == 0 && reorderedArgs.size() == argIndex.size() + 1 &&
      CAST(reorderedArgs.back().value, EllipsisExpr)) {
    isPartial = true;
    forceUnify(reorderedArgs.back().value, ctx->findInternal("void"));
    reorderedArgs.pop_back();
  } else if (reorderedArgs.size() + namedArgs.size() > argIndex.size()) {
    error("too many arguments for {} (expected {}, got {})", calleeType->toString(),
          argIndex.size(), reorderedArgs.size() + namedArgs.size());
  }

  if (ast) {
    ctx->addBlock();
    addFunctionGenerics(calleeType->getFunc());
  } else if (!ast && namedArgs.size()) {
    error("unexpected name '{}' (function pointers have argument names elided)",
          namedArgs.begin()->first);
  }

  for (int i = 0, ra = reorderedArgs.size(); i < argIndex.size(); i++) {
    if (i >= ra) {
      assert(ast);
      auto matchingName =
          ctx->cache->reverseIdentifierLookup[ast->args[argIndex[i]].name];
      auto it = namedArgs.find(matchingName);
      if (it != namedArgs.end()) {
        reorderedArgs.push_back({"", move(it->second)});
        namedArgs.erase(it);
      } else if (ast->args[argIndex[i]].deflt) {
        reorderedArgs.push_back({"", transform(ast->args[argIndex[i]].deflt)});
      } else {
        error("argument '{}' missing", matchingName);
      }
    }
    if (auto ee = CAST(reorderedArgs[i].value, EllipsisExpr))
      if (!ee->isPipeArg)
        pending.push_back(argIndex[i]);
  }
  for (auto &i : namedArgs)
    error(i.second, "unknown argument {}", i.first);
  if (isPartial || pending.size())
    pending.push_back(args.size());

  // Unification stage
  bool unificationsDone = true;
  for (int ri = 0; ri < reorderedArgs.size(); ri++) {
    auto sigType = calleeClass->args[argIndex[ri] + 1]->getClass();
    auto argType = reorderedArgs[ri].value->getType()->getClass();

    if (sigType && (sigType->isTrait || sigType->name == "Optional")) {
      // Case 0: type not yet known
      if (!argType) /* && !(reorderedArgs[ri].value->getType()->getUnbound() &&
                        reorderedArgs[ri]
                            .value->getType()
                            ->getUnbound()
                            ->treatAsClass)) { // do not unify if not yet known */
      {
        unificationsDone = false;
      }
      // Case 1: generator wrapping
      else if (sigType->name == "Generator" && argType &&
               argType->name != sigType->name && !extraStage) {
        // do not do this in pipelines
        reorderedArgs[ri].value = transform(
            N<CallExpr>(N<DotExpr>(move(reorderedArgs[ri].value), "__iter__")));
        forceUnify(reorderedArgs[ri].value,
                   calleeClass->args[argIndex[ri] + 1]); // sigType; needs Type* in
                                                         // unify for nicer interface
      }
      // Case 2: optional wrapping
      else if (sigType->name == "Optional" && argType &&
               argType->name != sigType->name) {
        if (extraStage && CAST(reorderedArgs[ri].value, EllipsisExpr)) {
          *extraStage = N<DotExpr>(N<IdExpr>("Optional"), "__new__");
          return expr->clone();
        } else {
          reorderedArgs[ri].value = transform(
              N<CallExpr>(N<IdExpr>("Optional"), move(reorderedArgs[ri].value)));
          forceUnify(reorderedArgs[ri].value, calleeClass->args[argIndex[ri] + 1]);
        }
      }
      // Case 3: Callables
      // TODO: this is only allowed with Seq function calls;
      // this won't be done with Function[] pointers or similar
      // as it is not trivial to cast Partial to Function[]
      else if (ast && startswith(sigType->name, "Function.N") && argType &&
               !startswith(argType->name, "Function.N")) {
        if (!startswith(argType->name, "Partial.N")) {
          reorderedArgs[ri].value =
              transform(N<DotExpr>(move(reorderedArgs[ri].value), "__call__"));
          argType = reorderedArgs[ri].value->getType()->getClass();
        }
        if (argType && startswith(argType->name, "Partial.N")) {
          forceUnify(argType->explicits[0].type, sigType->explicits[0].type);
          if (argType->explicits.size() != sigType->explicits.size() + 1)
            error("incompatible partial type");
          for (int j = 1; j < sigType->explicits.size(); j++)
            forceUnify(argType->explicits[j + 1].type, sigType->explicits[j].type);

          callee->getType()->getFunc()->args[ri + 1] =
              reorderedArgs[ri].value->getType(); // argType
        } else {
          forceUnify(reorderedArgs[ri].value, calleeClass->args[argIndex[ri] + 1]);
        }
      }
      // Otherwise, just unify as-is
      else {
        forceUnify(reorderedArgs[ri].value, calleeClass->args[argIndex[ri] + 1]);
      }
    } else if (sigType && argType && argType->name == "Optional") { // unwrap optional
      if (extraStage && CAST(reorderedArgs[ri].value, EllipsisExpr)) {
        *extraStage = N<IdExpr>("unwrap");
        return expr->clone();
      } else {
        reorderedArgs[ri].value =
            transform(N<CallExpr>(N<IdExpr>("unwrap"), move(reorderedArgs[ri].value)));
        forceUnify(reorderedArgs[ri].value, calleeClass->args[argIndex[ri] + 1]);
      }
    } else {
      forceUnify(reorderedArgs[ri].value, calleeClass->args[argIndex[ri] + 1]);
    }
  }
  if (ast)
    ctx->popBlock();

  // Realize functions that are passed as arguments
  auto fix = [&](ExprPtr &callee, const string &newName) {
    auto i = CAST(callee, IdExpr);
    if (!i || newName == i->value)
      return;
    auto comp = split(newName, ':');
    if (startswith(comp.back(), i->value))
      i->value = newName;
  };
  for (auto &ra : reorderedArgs)
    if (ra.value->getType()->getFunc() && ra.value->getType()->canRealize()) {
      auto r = realizeFunc(ra.value->getType());
      fix(ra.value, r->realizeString());
    }
  if (auto f = calleeType->getFunc()) {
    // Handle default generics (callee.g. foo[S, T=int])
    for (int i = 0; i < f->explicits.size(); i++)
      if (auto l = f->explicits[i].type->getLink()) {
        if (unificationsDone && l && l->kind == LinkType::Unbound &&
            ast->generics[i].deflt) {
          auto t = transformType(ast->generics[i].deflt);
          forceUnify(l, t->getType());
        }
      }
    if (f->canRealize()) {
      auto r = realizeFunc(f);
      if (knownTypes.empty())
        fix(callee, r->realizeString());
    }
  }

  // Emit final call
  if (pending.size()) { // (still) partial?
    pending.pop_back();
    string known(calleeClass->args.size() - 1, '1');
    for (auto p : pending)
      known[p] = '0';
    auto pt = generatePartialStub(known, knownTypes);
    vector<ExprPtr> a;
    a.push_back(move(callee));
    for (auto &r : reorderedArgs)
      if (!CAST(r.value, EllipsisExpr))
        a.push_back(move(r.value));
    callee = transform(N<CallExpr>(N<IdExpr>(pt), move(a)));
    forceUnify(expr, callee->getType());
    return callee;
  } else if (knownTypes.empty()) { // normal function
    callee = N<CallExpr>(move(callee), move(reorderedArgs));
    callee->setType(forceUnify(expr, calleeClass->args[0]));
    return callee;
  } else { // partial that is fulfilled
    callee = transform(
        N<CallExpr>(N<DotExpr>(move(callee), "__call__"), move(reorderedArgs)));
    forceUnify(expr, callee->getType());
    return callee;
  }
}

string TypecheckVisitor::patchIfRealizable(TypePtr typ, bool isClass) {
  if (typ->canRealize()) {
    if (isClass) {
      auto r = realizeType(typ->getClass());
      forceUnify(typ, r);
      return r->realizeString();
    } else if (typ->getFunc()) {
      auto r = realizeFunc(typ->getFunc());
      return r->realizeString();
    }
  }
  return "";
}

FuncTypePtr
TypecheckVisitor::findBestCall(ClassTypePtr c, const string &member,
                               const vector<pair<string, types::TypePtr>> &args,
                               bool failOnMultiple, types::TypePtr retType) {
  auto m = ctx->findMethod(c->name, member);
  if (m.empty())
    return nullptr;
  if (m.size() == 1) // works
    return m[0];

  // TODO: For now, overloaded functions are only possible in magic methods
  if (member.substr(0, 2) != "__" || member.substr(member.size() - 2) != "__")
    error("overloaded non-magic method {} in {}", member, c->toString());

  vector<pair<int, int>> scores;
  for (int i = 0; i < m.size(); i++) {
    auto mt = dynamic_pointer_cast<FuncType>(
        ctx->instantiate(getSrcInfo(), m[i], c.get(), false));

    vector<pair<string, TypePtr>> reorderedArgs;
    int s;
    if ((s = reorder(args, reorderedArgs, mt)) == -1)
      continue;

    for (int j = 0; j < reorderedArgs.size(); j++) {
      auto mac = mt->args[j + 1]->getClass();
      if (mac && mac->isTrait) // treat traits as generics
        continue;
      if (!reorderedArgs[j].second) // default arguments don't matter at all
        continue;
      auto ac = reorderedArgs[j].second->getClass();

      Type::Unification us;
      int u = reorderedArgs[j].second->unify(mt->args[j + 1], us);
      us.undo();
      if (u < 0) {
        if (mac && mac->name == "Optional" && ac && ac->name != mac->name) { // wrap
          int u = reorderedArgs[j].second->unify(mac->explicits[0].type, us);
          us.undo();
          if (u >= 0) {
            s += u + 2;
            continue;
          }
        }
        if (ac && ac->name == "Optional" && mac && ac->name != mac->name) { // unwrap
          int u = ac->explicits[0].type->unify(mt->args[j + 1], us);
          us.undo();
          if (u >= 0) {
            s += u;
            continue;
          }
        }
        s = -1;
        break;
      } else {
        s += u + 3;
      }
    }
    if (retType) {
      Type::Unification us;
      int u = retType->unify(mt->args[0], us);
      us.undo();
      s = u < 0 ? -1 : s + u;
    }
    if (s >= 0)
      scores.push_back({s, i});
  }
  if (!scores.size()) {
    return nullptr;
  }
  sort(scores.begin(), scores.end(), std::greater<pair<int, int>>());
  if (failOnMultiple) {
    // for (int i = 1; i < scores.size(); i++)
    //   if (scores[i].first == scores[0].first)
    //     // return nullptr;
    //     compilationWarning(format("multiple choices for magic call, selected
    //     {}",
    //                               (*m)[scores[0].second]->canonicalName),
    //                        getSrcInfo().file, getSrcInfo().line);
    //   else
    //     break;
  }
  return m[scores[0].second];
}

int TypecheckVisitor::reorder(const vector<pair<string, TypePtr>> &args,
                              vector<pair<string, TypePtr>> &reorderedArgs,
                              types::FuncTypePtr f) {
  vector<int> argIndex;
  for (int i = 0; i < int(f->args.size()) - 1; i++)
    argIndex.push_back(i);
  string knownTypes;

  bool namesStarted = false;
  unordered_map<string, TypePtr> namedArgs;
  for (int i = 0; i < args.size(); i++) {
    if (args[i].first == "" && namesStarted)
      error("unnamed argument after a named argument");
    namesStarted |= args[i].first != "";
    if (args[i].first == "")
      reorderedArgs.push_back({"", args[i].second});
    else if (namedArgs.find(args[i].first) == namedArgs.end())
      namedArgs[args[i].first] = args[i].second;
    else
      return -1;
  }

  if (reorderedArgs.size() + namedArgs.size() != argIndex.size())
    return -1;

  int score = reorderedArgs.size() * 2;

  FunctionStmt *ast = ctx->cache->functions[f->name].ast.get();
  seqassert(ast, "AST not accessible for {}", f->name);
  for (int i = 0, ra = reorderedArgs.size(); i < argIndex.size(); i++) {
    if (i >= ra) {
      assert(ast);
      auto matchingName =
          ctx->cache->reverseIdentifierLookup[ast->args[argIndex[i]].name];
      auto it = namedArgs.find(matchingName);
      if (it != namedArgs.end()) {
        reorderedArgs.push_back({"", it->second});
        namedArgs.erase(it);
        score += 2;
      } else if (ast->args[i].deflt) {
        if (ast->args[argIndex[i]].type) {
          reorderedArgs.push_back({"", f->args[argIndex[i] + 1]});
        } else { // TODO: does this even work? any dangling issues?
          // auto t = simplify(tmp->args[argIndex[i]].deflt);
          reorderedArgs.push_back({"", nullptr}); // really does not matter
        }
        score += 1;
      } else {
        return -1;
      }
    }
  }
  return score;
}

string TypecheckVisitor::generatePartialStub(const string &mask,
                                             const string &oldMask) {
  auto typeName = fmt::format("Partial.N{}", mask);
  if (ctx->cache->variardics.find(typeName) == ctx->cache->variardics.end()) {
    ctx->cache->variardics.insert(typeName);

    vector<Param> generics, args, missingArgs;
    vector<ExprPtr> genericNames, callArgs;
    args.emplace_back(Param{"ptr", nullptr, nullptr});
    missingArgs.push_back(Param{"self", nullptr, nullptr});
    for (int i = 0; i <= mask.size(); i++) {
      genericNames.push_back(N<IdExpr>(format("T{}", i)));
      generics.push_back(Param{format("T{}", i), nullptr, nullptr});
      if (i && mask[i - 1] == '1') {
        args.push_back(Param{format("a{0}", i), N<IdExpr>(format("T{}", i)), nullptr});
        callArgs.push_back(N<DotExpr>(N<IdExpr>("self"), format("a{0}", i)));
      } else if (i && mask[i - 1] == '0') {
        missingArgs.push_back(
            Param{format("a{0}", i), N<IdExpr>(format("T{}", i)), nullptr});
        callArgs.push_back(N<IdExpr>(format("a{0}", i)));
      }
    }
    args[0].type = N<IndexExpr>(N<IdExpr>(format("Function", mask.size())),
                                N<TupleExpr>(move(genericNames)));
    StmtPtr func =
        N<FunctionStmt>("__call__", N<IdExpr>("T0"), vector<Param>{}, move(missingArgs),
                        N<ReturnStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>("self"), "ptr"),
                                                  move(callArgs))),
                        vector<string>{});
    StmtPtr stmt = make_unique<ClassStmt>(
        typeName, move(generics), move(args), move(func),
        vector<string>{ATTR_TUPLE, "no_total_ordering", "no_pickle", "no_container",
                       "no_python"});

    stmt = SimplifyVisitor::apply(ctx->cache->imports[STDLIB_IMPORT].ctx, stmt,
                                  FILE_GENERATED);
    stmt = TypecheckVisitor(ctx).transform(stmt);
    prependStmts->push_back(move(stmt));
  }

  if (oldMask.empty())
    return typeName + ".__new__";

  auto fnName = format("{}.__new_{}_{}__", typeName, oldMask, mask);
  if (!ctx->find(fnName)) {
    vector<Param> args;
    vector<ExprPtr> newArgs;
    args.push_back(Param{"p", nullptr, nullptr});
    newArgs.push_back(N<DotExpr>(N<IdExpr>("p"), "ptr"));
    for (int i = 0; i < mask.size(); i++) {
      if (mask[i] == '1' && oldMask[i] == '0') {
        args.push_back(Param{format("a{}", i), nullptr, nullptr});
        newArgs.push_back(N<IdExpr>(format("a{}", i)));
      } else if (oldMask[i] == '1') {
        newArgs.push_back(N<DotExpr>(N<IdExpr>("p"), format("a{}", i + 1)));
      }
    }
    ExprPtr callee = N<DotExpr>(N<IdExpr>(typeName), "__new__");
    StmtPtr stmt = make_unique<FunctionStmt>(
        fnName, nullptr, vector<Param>{}, move(args),
        N<SuiteStmt>(N<ReturnStmt>(N<CallExpr>(move(callee), move(newArgs)))),
        vector<string>{});
    stmt = SimplifyVisitor::apply(ctx->cache->imports[STDLIB_IMPORT].ctx, stmt,
                                  FILE_GENERATED);
    stmt = TypecheckVisitor(ctx).transform(stmt);
    prependStmts->push_back(move(stmt));
  }
  return fnName;
}

} // namespace ast
} // namespace seq