/**
 * TODO : Redo error messages (right now they are awful)
 */

#include "util/fmt/format.h"
#include "util/fmt/ostream.h"
#include <deque>
#include <map>
#include <memory>
#include <ostream>
#include <stack>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/ast/ast.h"
#include "parser/ast/typecheck/typecheck.h"
#include "parser/ast/typecheck/typecheck_ctx.h"
#include "parser/ast/types.h"
#include "parser/common.h"
#include "parser/ocaml.h"

using fmt::format;
using std::deque;
using std::dynamic_pointer_cast;
using std::get;
using std::make_shared;
using std::make_unique;
using std::map;
using std::move;
using std::ostream;
using std::pair;
using std::shared_ptr;
using std::stack;
using std::static_pointer_cast;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::unordered_set;
using std::vector;

namespace seq {
namespace ast {

using namespace types;

string printParents(types::TypePtr t) {
  string s;
  for (auto p = t; p; p = p = t->getClass()->parent) {
    s = t->toString() + ":" + s;
  }
  return ":" + s;
}

TypecheckVisitor::TypecheckVisitor(shared_ptr<TypeContext> ctx,
                                   shared_ptr<vector<StmtPtr>> stmts)
    : ctx(ctx) {
  prependStmts = stmts ? stmts : make_shared<vector<StmtPtr>>();
}

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

PatternPtr TypecheckVisitor::transform(const PatternPtr &pat) {
  if (!pat)
    return nullptr;
  TypecheckVisitor v(ctx, prependStmts);
  v.setSrcInfo(pat->getSrcInfo());
  pat->accept(v);
  return move(v.resultPattern);
}

void TypecheckVisitor::prepend(StmtPtr s) {
  if (auto t = transform(s))
    prependStmts->push_back(move(t));
}

StmtPtr TypecheckVisitor::apply(shared_ptr<Cache> cache, StmtPtr stmts) {
  auto ctx = make_shared<TypeContext>(cache);
  ctx->add(TypecheckItem::Type, "void", make_shared<ClassType>("void", true));
  TypecheckVisitor v(ctx);
  return v.realizeBlock(stmts, true);
}

/*************************************************************************************/

void TypecheckVisitor::visit(const BoolExpr *expr) {
  resultExpr = expr->clone();
  resultExpr->setType(ctx->findInternal(".bool"));
}

void TypecheckVisitor::visit(const IntExpr *expr) {
  resultExpr = expr->clone();
  resultExpr->setType(ctx->findInternal(".int"));
}

void TypecheckVisitor::visit(const FloatExpr *expr) {
  resultExpr = expr->clone();
  resultExpr->setType(ctx->findInternal(".float"));
}

void TypecheckVisitor::visit(const StringExpr *expr) {
  resultExpr = expr->clone();
  resultExpr->setType(ctx->findInternal(".str"));
}

void TypecheckVisitor::visit(const IdExpr *expr) {
  auto val = ctx->find(expr->value);
  if (!val)
    ctx->dump();
  assert(val);
  if (val->isStatic()) {
    auto s = val->getType()->getStatic();
    assert(s);
    resultExpr = transform(N<IntExpr>(s->getValue()));
  } else {
    resultExpr = expr->clone();
    TypePtr typ = val->getType();
    if (val->isType())
      resultExpr->markType();
    else
      typ = ctx->instantiate(getSrcInfo(), val->getType());
    resultExpr->setType(forceUnify(resultExpr, typ));
    auto newName = patchIfRealizable(typ, val->isType());
    if (!newName.empty())
      static_cast<IdExpr *>(resultExpr.get())->value = newName;
  }
}

void TypecheckVisitor::visit(const IfExpr *expr) {
  auto e =
      N<IfExpr>(transform(expr->cond), transform(expr->eif), transform(expr->eelse));
  e->setType(forceUnify(expr, e->eif->getType()));
  resultExpr = move(e);
}

void TypecheckVisitor::visit(const BinaryExpr *expr) {
  auto magics = unordered_map<string, string>{
      {"+", "add"},     {"-", "sub"},     {"*", "mul"}, {"**", "pow"}, {"/", "truediv"},
      {"//", "div"},    {"@", "mathmul"}, {"%", "mod"}, {"<", "lt"},   {"<=", "le"},
      {">", "gt"},      {">=", "ge"},     {"==", "eq"}, {"!=", "ne"},  {"<<", "lshift"},
      {">>", "rshift"}, {"&", "and"},     {"|", "or"},  {"^", "xor"}};
  auto le = transform(expr->lexpr);
  auto re = transform(expr->rexpr);
  if (le->getType()->getUnbound() || re->getType()->getUnbound()) {
    resultExpr = N<BinaryExpr>(move(le), expr->op, move(re));
    resultExpr->setType(
        forceUnify(expr, ctx->addUnbound(getSrcInfo(), ctx->getLevel())));
  } else if (expr->op == "is") {
    if (!le->getType()->canRealize() || !le->getType()->canRealize()) {
      resultExpr = N<BinaryExpr>(move(le), expr->op, move(re));
    } else {
      auto lc = realizeType(le->getType()->getClass());
      auto rc = realizeType(re->getType()->getClass());
      if (!lc || !rc)
        error("both sides of 'is' expression must be of same reference type");
      resultExpr =
          transform(N<BinaryExpr>(N<CallExpr>(N<DotExpr>(move(le), "__raw__")),
                                  "==", N<CallExpr>(N<DotExpr>(move(re), "__raw__"))));
    }
    resultExpr->setType(forceUnify(expr, ctx->findInternal(".bool")));
  } else {
    auto mi = magics.find(expr->op);
    if (mi == magics.end())
      error("invalid binary operator '{}'", expr->op);
    auto magic = mi->second;
    auto lc = le->getType()->getClass(), rc = re->getType()->getClass();
    assert(lc && rc);
    if (findBestCall(lc, format("__{}__", magic), {{"", lc}, {"", rc}})) {
      if (expr->inPlace &&
          findBestCall(lc, format("__i{}__", magic), {{"", lc}, {"", rc}}))
        magic = "i" + magic;
    } else if (findBestCall(rc, format("__r{}__", magic), {{"", rc}, {"", lc}})) {
      magic = "r" + magic;
    } else {
      error("cannot find magic '{}' for {}", magic, lc->toString());
    }
    magic = format("__{}__", magic);
    resultExpr = transform(
        N<CallExpr>(N<DotExpr>(expr->lexpr->clone(), magic), expr->rexpr->clone()));
    forceUnify(expr, resultExpr->getType());
  }
}

void TypecheckVisitor::visit(const PipeExpr *expr) {
  auto extractType = [&](TypePtr t) {
    auto c = t->getClass();
    if (c && c->name == ".Generator")
      return c->explicits[0].type;
    else
      return t;
  };
  auto updateType = [&](TypePtr t, int inTypePos, ExprPtr &fe) {
    auto f = fe->getType()->getClass();
    assert(f && f->getCallable());
    bool isPartial = f->name.substr(0, 9) == ".Partial.";
    if (isPartial) {
      int j = 0;
      for (int i = 9; i < f->name.size(); i++)
        if (f->name[i] == '0') {
          if (j == inTypePos) {
            j = i - 9;
            break;
          }
          j++;
        }
      inTypePos = j;
    }
    f = f->getCallable()->getClass();
    forceUnify(t, f->args[inTypePos + 1]);
    if (f->canRealize() && f->getFunc()) {
      auto t = realizeFunc(f->getFunc());
      if (isPartial)
        fixExprName(fe, t->realizeString());
    }
    return f->args[0];
  };

  vector<PipeExpr::Pipe> items;
  items.push_back({expr->items[0].op, transform(expr->items[0].expr)});
  vector<types::TypePtr> types;
  TypePtr inType = nullptr;
  inType = extractType(items.back().expr->getType());
  types.push_back(inType);
  int inTypePos = 0;
  for (int i = 1; i < expr->items.size(); i++) {
    auto &l = expr->items[i];
    if (auto ce = CAST(l.expr, CallExpr)) {
      int inTypePos = -1;
      for (int ia = 0; ia < ce->args.size(); ia++)
        if (CAST(ce->args[ia].value, EllipsisExpr)) {
          if (inTypePos == -1)
            inTypePos = ia;
          else
            error(ce->args[ia].value, "unexpected partial argument");
        }
      if (inTypePos == -1) {
        ce->args.insert(ce->args.begin(), {"", N<EllipsisExpr>()});
        inTypePos = 0;
      }
      items.push_back({l.op, transform(l.expr)});
    } else {
      items.push_back(
          {l.op, transform(N<CallExpr>(transform(l.expr), N<EllipsisExpr>()))});
      inTypePos = 0;
    }
    inType = updateType(inType, inTypePos, items.back().expr);
    types.push_back(inType);
    if (i < expr->items.size() - 1)
      inType = extractType(inType);
  }
  resultExpr = N<PipeExpr>(move(items));
  CAST(resultExpr, PipeExpr)->inTypes = types;
  resultExpr->setType(forceUnify(expr, inType));
}

void TypecheckVisitor::visit(const InstantiateExpr *expr) {
  ExprPtr e = transform(expr->type, true);
  auto g = ctx->instantiate(e->getSrcInfo(), e->getType());
  for (int i = 0; i < expr->params.size(); i++) {
    TypePtr t = nullptr;
    if (auto s = CAST(expr->params[i], StaticExpr)) {
      map<string, Generic> m;
      for (auto g : s->captures) {
        auto val = ctx->find(g);
        assert(val && val->isStatic());
        auto t = val->getType()->follow();
        m[g] = {g, t,
                t->getLink()
                    ? t->getLink()->id
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
      t = ctx->instantiate(getSrcInfo(), transformType(expr->params[i])->getType());
    }
    /// Note: at this point, only single-variable static var expression (e.g.
    /// N) is allowed, so unify will work as expected.
    if (g->getFunc())
      forceUnify(g->getFunc()->explicits[i].type, t);
    else
      forceUnify(g->getClass()->explicits[i].type, t);
  }
  bool isType = e->isType();
  auto t = forceUnify(expr, g);
  auto newName = patchIfRealizable(t, isType);
  if (!newName.empty())
    fixExprName(e, newName);
  resultExpr = move(e); // will get replaced by identifier later on
  if (isType)
    resultExpr->markType();
  resultExpr->setType(t);
}

void TypecheckVisitor::visit(const IndexExpr *expr) {
  auto getTupleIndex = [&](auto tuple, const auto &expr, const auto &index) -> ExprPtr {
    if (!startswith(tuple->name, ".Tuple."))
      return nullptr;
    seq_int_t s = 0, e = tuple->args.size(), st = 1;
    if (getInt(&s, index)) {
      int i = translateIndex(s, e);
      if (i < 0 || i >= e)
        error("tuple index out of range (expected 0..{}, got {})", e, i);
      return transform(N<DotExpr>(clone(expr), format("a{}", i + 1)));
    } else if (auto i = CAST(index, SliceExpr)) {
      if (!getInt(&s, i->st) || !getInt(&e, i->ed) || !getInt(&st, i->step))
        return nullptr;
      sliceAdjustIndices(tuple->args.size(), &s, &e, st);
      vector<ExprPtr> te;
      for (auto i = s; (st >= 0) ? (i < e) : (i >= e); i += st) {
        if (i < 0 || i >= e)
          error("tuple index out of range (expected 0..{}, got {})", e, i);
        te.push_back(N<DotExpr>(clone(expr), format("a{}", i + 1)));
      }
      return transform(N<TupleExpr>(move(te)));
    }
    return nullptr;
  };

  ExprPtr e = transform(expr->expr, true);
  if (auto c = e->getType()->getClass()) {
    resultExpr = getTupleIndex(c, expr->expr, expr->index);
    if (!resultExpr)
      resultExpr = transform(N<CallExpr>(N<DotExpr>(expr->expr->clone(), "__getitem__"),
                                         expr->index->clone()));
  } else {
    resultExpr = N<IndexExpr>(move(e), transform(expr->index));
    resultExpr->setType(expr->getType()
                            ? expr->getType()
                            : ctx->addUnbound(getSrcInfo(), ctx->getLevel()));
  }
}

void TypecheckVisitor::visit(const StackAllocExpr *expr) {
  auto te = transformType(expr->typeExpr);
  auto e = transform(expr->expr);

  auto t = te->getType();
  resultExpr = N<StackAllocExpr>(move(te), move(e));
  t = ctx->instantiateGeneric(expr->getSrcInfo(), ctx->findInternal(".Array"), {t});
  patchIfRealizable(t, true);
  resultExpr->setType(forceUnify(expr, t));
}

void TypecheckVisitor::visit(const CallExpr *expr) {
  ExprPtr e = nullptr;
  vector<CallExpr::Arg> args;
  for (auto &i : expr->args)
    args.push_back({i.name, transform(i.value)});

  // Intercept obj.foo() calls and transform obj.foo(...) to foo(obj, ...)
  if (auto d = CAST(expr->expr, DotExpr)) {
    auto dotlhs = transform(d->expr, true);
    if (auto c = dotlhs->getType()->getClass()) {
      vector<pair<string, TypePtr>> targs;
      if (!dotlhs->isType())
        targs.push_back({"", c});
      for (auto &a : args)
        targs.push_back({a.name, a.value->getType()});
      if (auto m = findBestCall(c, d->member, targs, true)) {
        if (!dotlhs->isType())
          args.insert(args.begin(), {"", move(dotlhs)});
        e = N<IdExpr>(m->name);
        e->setType(ctx->instantiate(getSrcInfo(), m, c));
      } else {
        error("cannot find method '{}' in {} with arguments {}", d->member,
              c->toString(), v2s(targs));
      }
    }
  }
  if (!e)
    e = transform(expr->expr, true);
  forceUnify(expr->expr.get(), e->getType());

  // TODO: optional promition in findBestCall
  if (e->isType()) {                   // Replace constructor with appropriate calls
    auto c = e->getType()->getClass(); // no need for instantiation
    assert(c);
    if (c->isRecord()) {
      vector<TypePtr> targs;
      for (auto &a : args)
        targs.push_back(a.value->getType());
      resultExpr = transform(N<CallExpr>(N<DotExpr>(move(e), "__new__"), move(args)));
    } else {
      string var = getTemporaryVar("typ");
      /// TODO: assumes that a class cannot have multiple __new__ magics
      /// WARN: passing e & args that have already been transformed
      prepend(
          N<AssignStmt>(N<IdExpr>(var), N<CallExpr>(N<DotExpr>(move(e), "__new__"))));
      prepend(
          N<ExprStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "__init__"), move(args))));
      resultExpr = transform(N<IdExpr>(var));
    }
    return;
  }

  auto c = e->getType();
  if (!c) { // Unbound caller, will be handled later
    resultExpr = N<CallExpr>(move(e), move(args));
    resultExpr->setType(expr->getType()
                            ? expr->getType()
                            : ctx->addUnbound(getSrcInfo(), ctx->getLevel()));
    return;
  }
  if (c->getClass() && !c->getClass()->getCallable()) { // route to a call method
    resultExpr = transform(N<CallExpr>(N<DotExpr>(move(e), "__call__"), move(args)));
    return;
  }

  // Handle named and default arguments
  vector<CallExpr::Arg> reorderedArgs;
  vector<int> availableArguments;
  bool isPartial = false;
  string knownTypes;
  if (auto cc = dynamic_pointer_cast<types::ClassType>(c)) {
    if (cc->name.substr(0, 9) == ".Partial.") {
      isPartial = true;
      knownTypes = cc->name.substr(9);
      c = c->getClass()->getCallable();
      assert(c);
    }
  }
  auto cc = c->getClass();
  auto &t_args = cc->args;
  for (int i = 0; i < int(t_args.size()) - 1; i++)
    if (!isPartial || knownTypes[i] == '0')
      availableArguments.push_back(i);
  auto pending = callFunc(c, args, reorderedArgs, availableArguments);

  // Realize functions that are passed as arguments
  for (auto &ra : reorderedArgs)
    if (ra.value->getType()->canRealize()) {
      if (auto f = ra.value->getType()->getFunc()) {
        auto r = realizeFunc(f);
        fixExprName(ra.value, r->realizeString());
      }
      // TODO: realize partials
    }

  if (c->canRealize() && c->getFunc()) {
    auto r = realizeFunc(c->getFunc());
    if (!isPartial)
      fixExprName(e, r->realizeString());
  }
  TypePtr t = make_shared<LinkType>(t_args[0]);
  if (pending.size()) {
    pending.pop_back();
    string known(t_args.size() - 1, '1');
    for (auto p : pending)
      known[p] = '0';

    if (ctx->cache->partials.find(known) == ctx->cache->partials.end()) {
      ctx->cache->partials[known] = make_shared<ClassType>(
          fmt::format(".Partial.{}", known), true, vector<TypePtr>{},
          vector<Generic>{Generic{
              "F",
              make_shared<types::LinkType>(LinkType::Generic, ctx->cache->unboundCount),
              ctx->cache->unboundCount}});
      ctx->cache->unboundCount++;
    }
    t = ctx->instantiateGeneric(getSrcInfo(), ctx->cache->partials[known], {c});
  }
  resultExpr = N<CallExpr>(move(e), move(reorderedArgs));
  resultExpr->setType(forceUnify(expr, t));
}

void TypecheckVisitor::visit(const DotExpr *expr) {
  auto lhs = transform(expr->expr, true);
  TypePtr typ = nullptr;
  if (lhs->getType()->getUnbound()) {
    typ = expr->getType() ? expr->getType()
                          : ctx->addUnbound(getSrcInfo(), ctx->getLevel());
  } else if (auto c = lhs->getType()->getClass()) {
    if (auto m = ctx->findMethod(c->name, expr->member)) {
      if (m->size() > 1)
        error("ambigious partial expression"); /// TODO
      if (lhs->isType()) {
        auto name = (*m)[0]->name;
        auto val = ctx->find(name);
        assert(val);
        auto t = ctx->instantiate(getSrcInfo(), (*m)[0], c);
        resultExpr = N<IdExpr>(name);
        resultExpr->setType(t);
        auto newName = patchIfRealizable(t, val->isType());
        if (!newName.empty())
          static_cast<IdExpr *>(resultExpr.get())->value = newName;
        return;
      } else { // cast y.foo to CLS.foo(y, ...)
        auto f = (*m)[0];
        vector<ExprPtr> args;
        args.push_back(move(lhs));
        for (int i = 0; i < std::max(1, (int)f->args.size() - 2); i++)
          args.push_back(N<EllipsisExpr>());

        auto ast = (FunctionStmt *)(ctx->cache->asts[f->name].get());
        if (in(ast->attributes, "property"))
          args.pop_back();
        resultExpr = transform(N<CallExpr>(N<IdExpr>((*m)[0]->name), move(args)));
        return;
      }
    } else if (auto mm = ctx->findMember(c->name, expr->member)) {
      typ = ctx->instantiate(getSrcInfo(), mm, c);
    } else {
      error("cannot find '{}' in {}", expr->member, lhs->getType()->toString());
    }
  } else {
    error("cannot find '{}' in {}", expr->member, lhs->getType()->toString());
  }

  resultExpr = N<DotExpr>(move(lhs), expr->member);
  resultExpr->setType(forceUnify(expr, typ));
}

void TypecheckVisitor::visit(const EllipsisExpr *expr) {
  resultExpr = N<EllipsisExpr>();
  resultExpr->setType(ctx->addUnbound(getSrcInfo(), ctx->getLevel()));
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
                                               ctx->findInternal(".Ptr"), {t})));
}

void TypecheckVisitor::visit(const YieldExpr *expr) {
  resultExpr = N<YieldExpr>();
  if (!ctx->getLevel() || !ctx->bases.back().type->getFunc())
    error("(yield) cannot be used outside of functions");
  auto t = ctx->instantiateGeneric(getSrcInfo(), ctx->findInternal(".Generator"),
                                   {ctx->addUnbound(getSrcInfo(), ctx->getLevel())});
  auto &base = ctx->bases.back();
  if (base.returnType)
    t = forceUnify(base.returnType, t);
  else
    base.returnType = t;
  auto c = t->follow()->getClass();
  assert(c);
  resultExpr->setType(forceUnify(expr, c->explicits[0].type));
}

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
  auto l = CAST(stmt->lhs, IdExpr);
  assert(l);

  auto rhs = transform(stmt->rhs);
  auto typExpr = transformType(stmt->type);
  if (typExpr && typExpr->getType()->getClass()) {
    auto typ = ctx->instantiate(getSrcInfo(), typExpr->getType());
    if (!wrapOptional(typ, rhs))
      forceUnify(typ, rhs->getType());
  }

  ctx->add(rhs->isType()
               ? TypecheckItem::Type
               : rhs->getType()->getFunc() ? TypecheckItem::Func : TypecheckItem::Var,
           l->value, rhs->getType(), l->value[0] == '.');
  if (l->value[0] == '.')
    ctx->cache->astTypes[l->value] = rhs->getType();
  resultStmt = N<AssignStmt>(clone(stmt->lhs), move(rhs), move(typExpr));
}

void TypecheckVisitor::visit(const UpdateStmt *stmt) {
  auto l = transform(stmt->lhs);
  auto r = transform(stmt->rhs);
  forceUnify(l->getType(), r->getType());
  if (!wrapOptional(l->getType(), r))
    stmt->lhs->setType(forceUnify(r.get(), l->getType()));
  resultStmt = N<UpdateStmt>(move(l), move(r));
}

void TypecheckVisitor::visit(const AssignMemberStmt *stmt) {
  auto lh = transform(stmt->lhs);
  auto rh = transform(stmt->rhs);
  auto c = lh->getType()->getClass();
  if (c && c->isRecord())
    error("records are read-only ^ {} , {}", c->toString(), lh->toString());
  auto mm = ctx->findMember(c->name, stmt->member);
  forceUnify(ctx->instantiate(getSrcInfo(), mm, c), rh->getType());
  resultStmt = N<AssignMemberStmt>(move(lh), stmt->member, move(rh));
}

void TypecheckVisitor::visit(const ReturnStmt *stmt) {
  if (stmt->expr) {
    auto e = transform(stmt->expr);
    auto &base = ctx->bases.back();
    if (base.returnType)
      forceUnify(e->getType(), base.returnType);
    else
      base.returnType = e->getType();
    resultStmt = N<ReturnStmt>(move(e));
  } else {
    resultStmt = N<ReturnStmt>(nullptr);
  }
}

void TypecheckVisitor::visit(const YieldStmt *stmt) {
  types::TypePtr t = nullptr;
  if (stmt->expr) {
    auto e = transform(stmt->expr);
    t = ctx->instantiateGeneric(e->getSrcInfo(), ctx->findInternal(".Generator"),
                                {e->getType()});
    resultStmt = N<YieldStmt>(move(e));
  } else {
    t = ctx->instantiateGeneric(stmt->getSrcInfo(), ctx->findInternal(".Generator"),
                                {ctx->findInternal("void")});
    resultStmt = N<YieldStmt>(nullptr);
  }
  auto &base = ctx->bases.back();
  if (base.returnType)
    forceUnify(t, base.returnType);
  else
    base.returnType = t;
}

void TypecheckVisitor::visit(const AssertStmt *stmt) {
  resultStmt = N<AssertStmt>(transform(stmt->expr));
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
  TypePtr varType = nullptr;
  varType = ctx->addUnbound(stmt->var->getSrcInfo(), ctx->getLevel());
  if (!iter->getType()->getUnbound()) {
    auto iterType = iter->getType()->getClass();
    if (!iterType || iterType->name != ".Generator")
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
  auto oldMatchType = ctx->matchType;
  ctx->matchType = w->getType();
  vector<PatternPtr> patterns;
  vector<StmtPtr> cases;
  for (auto ci = 0; ci < stmt->cases.size(); ci++) {
    ctx->addBlock();
    if (auto p = CAST(stmt->patterns[ci], BoundPattern)) {
      auto boundPat = transform(p->pattern);
      ctx->add(TypecheckItem::Var, p->var, boundPat->getType());
      patterns.push_back(move(boundPat));
      cases.push_back(transform(stmt->cases[ci]));
    } else {
      patterns.push_back(transform(stmt->patterns[ci]));
      cases.push_back(transform(stmt->cases[ci]));
    }
    ctx->popBlock();
  }
  ctx->matchType = oldMatchType;
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
                               nullptr, stmt->attributes);
  bool isClassMember = ctx->getLevel() && !ctx->bases.back().type->getFunc();
  auto it = ctx->cache->astTypes.find(stmt->name);
  if (it != ctx->cache->astTypes.end())
    return;

  auto t = make_shared<FuncType>(
      stmt->name,
      ctx->findInternal(format(".Function.{}", stmt->args.size()))->getClass());

  ctx->bases.push_back({t});
  ctx->addBlock();
  t->explicits = parseGenerics(stmt->generics, ctx->getLevel() - 1); // level down
  vector<TypePtr> generics;
  for (auto &i : stmt->generics)
    generics.push_back(ctx->find(i.name)->getType());
  if (stmt->ret) {
    t->args.push_back(transformType(stmt->ret)->getType());
  } else {
    t->args.push_back(ctx->addUnbound(getSrcInfo(), ctx->getLevel()));
    generics.push_back(t->args.back());
  }
  for (auto &a : stmt->args) {
    t->args.push_back(a.type ? transformType(a.type)->getType()
                             : ctx->addUnbound(getSrcInfo(), ctx->getLevel()));
    if (!a.type)
      generics.push_back(t->args.back());
    ctx->add(TypecheckItem::Var, a.name, t->args.back());
  }
  ctx->bases.pop_back();
  if (stmt->name == ".unwrap")
    assert(1);
  for (auto &g : generics) { // Generalize generics
    assert(g && g->getLink() && g->getLink()->kind != types::LinkType::Link);
    if (g->getLink()->kind == LinkType::Unbound)
      g->getLink()->kind = LinkType::Generic;
  }
  ctx->popBlock();
  if (ctx->getLevel())
    t->parent = ctx->bases.back().type;
  if (isClassMember && !in(stmt->attributes, ".method")) {
    t->codegenParent = t->parent;
    t->parent = ctx->getLevel() > 1 ? ctx->bases[ctx->bases.size() - 2].type : nullptr;
  }

  t->setSrcInfo(stmt->getSrcInfo());
  t = std::static_pointer_cast<FuncType>(t->generalize(ctx->getLevel()));
  LOG7("[stmt] added func {}: {} (base={}; parent={})", stmt->name, t->toString(),
       ctx->getBase(true), printParents(t->parent));
  ctx->cache->astTypes[stmt->name] = t;

  if ((in(stmt->attributes, "builtin") || in(stmt->attributes, ".c")) &&
      !t->canRealize())
    error("builtins and external functions must be realizable");
  // Class members are realized after the class is sealed to prevent premature
  // unification of class generics
  if (!isClassMember && t->canRealize())
    realizeFunc(ctx->instantiate(getSrcInfo(), t)->getFunc());
}

void TypecheckVisitor::visit(const ClassStmt *stmt) {
  resultStmt = N<ClassStmt>(stmt->isRecord, stmt->name, vector<Param>(),
                            vector<Param>(), N<SuiteStmt>(), stmt->attributes);
  auto it = ctx->cache->astTypes.find(stmt->name);
  if (it != ctx->cache->astTypes.end())
    return;

  vector<StmtPtr> stmts;
  stmts.push_back(move(resultStmt));

  auto ct = make_shared<ClassType>(stmt->name, stmt->isRecord, vector<TypePtr>(),
                                   vector<Generic>(),
                                   ctx->getLevel() ? ctx->bases.back().type : nullptr);
  ct->setSrcInfo(stmt->getSrcInfo());
  auto ctxi = make_shared<TypecheckItem>(TypecheckItem::Type, ct, ctx->getBase(), true);
  if (!stmt->isRecord) // add classes early
    ctx->add(stmt->name, ctxi);
  ctx->cache->astTypes[stmt->name] = ct;

  ctx->bases.push_back({ct});
  ct->explicits = parseGenerics(stmt->generics, ctx->getLevel() - 1);
  for (auto &a : stmt->args) {
    auto t = transformType(a.type)->getType()->generalize(ctx->getLevel() - 1);
    ctx->cache->classMembers[stmt->name].push_back({a.name, t});
    if (stmt->isRecord)
      ct->args.push_back(t);
  }
  if (stmt->isRecord)
    ctx->add(stmt->name, ctxi);
  if (stmt->suite)
    for (auto &s : ((SuiteStmt *)(stmt->suite.get()))->stmts) {
      auto t = transform(s);
      auto f = CAST(t, FunctionStmt)->name;
      ctx->cache->classMethods[stmt->name][ctx->cache->reverseLookup[f]].push_back(
          ctx->cache->astTypes[f]->getFunc());
      stmts.push_back(move(t));
    }
  ctx->bases.pop_back();

  for (auto &g : stmt->generics) { // Generalize in place
    auto val = ctx->find(g.name);
    if (auto g = val->getType()) {
      assert(g && g->getLink() && g->getLink()->kind != types::LinkType::Link);
      if (g->getLink()->kind == LinkType::Unbound)
        g->getLink()->kind = LinkType::Generic;
    }
    ctx->remove(g.name);
  }

  LOG7("[class] {} (base={}, parent={})", ct->toString(), ctx->getBase(true),
       printParents(ct->parent));
  for (auto &m : ctx->cache->classMembers[stmt->name])
    LOG7("       - member: {}: {}", m.first, m.second->toString());
  for (auto &m : ctx->cache->classMethods[stmt->name])
    for (auto &f : m.second) {
      if (f->canRealize())
        realizeFunc(ctx->instantiate(getSrcInfo(), f)->getFunc());
      LOG7("       - method: {}: {}", m.first, f->toString());
    }
  resultStmt = N<SuiteStmt>(move(stmts));
}

void TypecheckVisitor::visit(const ExtendStmt *stmt) {
  auto i = CAST(stmt->type, IdExpr);
  assert(i);
  auto val = ctx->find(i->value);
  assert(val && val->isType());
  auto ct = val->getType()->getClass();
  ctx->bases.push_back({ct});
  ctx->addBlock();
  for (int i = 0; i < stmt->generics.size(); i++) {
    auto l = ct->explicits[i].type->getLink();
    assert(l);
    ctx->add(TypecheckItem::Type, stmt->generics[i],
             make_shared<LinkType>(LinkType::Unbound, ct->explicits[i].id,
                                   ctx->getLevel() - 1, nullptr, l->isStatic),
             false, true, l->isStatic);
  }
  vector<StmtPtr> stmts;
  for (auto &s : ((SuiteStmt *)(stmt->suite.get()))->stmts) {
    auto t = transform(s);
    auto f = CAST(t, FunctionStmt)->name;
    ctx->cache->classMethods[i->value][ctx->cache->reverseLookup[f]].push_back(
        ctx->cache->astTypes[f]->getFunc());
    stmts.push_back(move(t));
  }
  for (int i = 0; i < stmt->generics.size(); i++)
    if (ct->explicits[i].type) {
      auto t = ctx->find(stmt->generics[i])->getType()->getLink();
      assert(t && t->kind == LinkType::Unbound);
      t->kind = LinkType::Generic;
    }
  ctx->bases.pop_back();
  ctx->popBlock();
  resultStmt = N<SuiteStmt>(move(stmts));
}

void TypecheckVisitor::visit(const StarPattern *pat) {
  resultPattern = N<StarPattern>();

  resultPattern->setType(forceUnify(
      pat, forceUnify(ctx->matchType, ctx->addUnbound(getSrcInfo(), ctx->getLevel()))));
}

void TypecheckVisitor::visit(const IntPattern *pat) {
  resultPattern = N<IntPattern>(pat->value);

  resultPattern->setType(
      forceUnify(pat, forceUnify(ctx->matchType, ctx->findInternal(".int"))));
}

void TypecheckVisitor::visit(const BoolPattern *pat) {
  resultPattern = N<BoolPattern>(pat->value);

  resultPattern->setType(
      forceUnify(pat, forceUnify(ctx->matchType, ctx->findInternal(".bool"))));
}

void TypecheckVisitor::visit(const StrPattern *pat) {
  resultPattern = N<StrPattern>(pat->value);

  resultPattern->setType(
      forceUnify(pat, forceUnify(ctx->matchType, ctx->findInternal(".str"))));
}

void TypecheckVisitor::visit(const SeqPattern *pat) {
  resultPattern = N<SeqPattern>(pat->value);

  resultPattern->setType(
      forceUnify(pat, forceUnify(ctx->matchType, ctx->findInternal(".seq"))));
}

void TypecheckVisitor::visit(const RangePattern *pat) {
  resultPattern = N<RangePattern>(pat->start, pat->end);

  resultPattern->setType(
      forceUnify(pat, forceUnify(ctx->matchType, ctx->findInternal(".int"))));
}

void TypecheckVisitor::visit(const TuplePattern *pat) {
  auto p = N<TuplePattern>(transform(pat->patterns));
  TypePtr t = nullptr;
  vector<TypePtr> types;
  for (auto &pp : p->patterns)
    types.push_back(pp->getType());
  // TODO: Ensure type...
  error("not yet implemented");
  t = make_shared<ClassType>("Tuple", true, types);
  resultPattern = move(p);
  resultPattern->setType(forceUnify(pat, forceUnify(ctx->matchType, t)));
}

void TypecheckVisitor::visit(const ListPattern *pat) {
  auto p = N<ListPattern>(transform(pat->patterns));
  TypePtr t = ctx->addUnbound(getSrcInfo(), ctx->getLevel());
  for (auto &pp : p->patterns)
    forceUnify(t, pp->getType());
  t = ctx->instantiateGeneric(getSrcInfo(), ctx->findInternal(".list"), {t});
  resultPattern = move(p);
  resultPattern->setType(forceUnify(pat, forceUnify(ctx->matchType, t)));
}

void TypecheckVisitor::visit(const OrPattern *pat) {
  auto p = N<OrPattern>(transform(pat->patterns));
  assert(p->patterns.size());
  TypePtr t = p->patterns[0]->getType();
  for (auto &pp : p->patterns)
    forceUnify(t, pp->getType());
  resultPattern = move(p);
  resultPattern->setType(forceUnify(pat, forceUnify(ctx->matchType, t)));
}

void TypecheckVisitor::visit(const WildcardPattern *pat) {
  resultPattern = N<WildcardPattern>(pat->var);
  if (pat->var != "")
    ctx->add(TypecheckItem::Var, pat->var, ctx->matchType);
  resultPattern->setType(forceUnify(pat, ctx->matchType));
}

void TypecheckVisitor::visit(const GuardedPattern *pat) {
  auto p = N<GuardedPattern>(transform(pat->pattern), transform(pat->cond));
  auto t = p->pattern->getType();
  resultPattern = move(p);
  resultPattern->setType(forceUnify(pat, forceUnify(ctx->matchType, t)));
}

void TypecheckVisitor::visit(const BoundPattern *pat) {
  auto p = N<BoundPattern>(pat->var, transform(pat->pattern));
  auto t = p->pattern->getType();
  ctx->add(TypecheckItem::Var, p->var, t);
  resultPattern = move(p);
  resultPattern->setType(forceUnify(pat, forceUnify(ctx->matchType, t)));
}

/*******************************/

StaticVisitor::StaticVisitor(std::map<string, types::Generic> &m)
    : generics(m), evaluated(false), value(0) {}

pair<bool, int> StaticVisitor::transform(const ExprPtr &e) {
  StaticVisitor v(generics);
  e->accept(v);
  return {v.evaluated, v.evaluated ? v.value : -1};
}

void StaticVisitor::visit(const IdExpr *expr) {
  auto val = generics.find(expr->value);
  auto t = val->second.type->follow();
  if (t->getLink()) {
    evaluated = false;
  } else {
    assert(t->getStatic() && t->getStatic()->explicits.size() <= 1);
    evaluated = t->canRealize();
    if (evaluated)
      value = t->getStatic()->getValue();
  }
}

void StaticVisitor::visit(const IntExpr *expr) {
  evaluated = true;
  value = std::stoull(expr->value, nullptr, 0);
}

void StaticVisitor::visit(const UnaryExpr *expr) {
  std::tie(evaluated, value) = transform(expr->expr);
  if (evaluated) {
    if (expr->op == "-")
      value = -value;
    else if (expr->op == "!")
      value = !bool(value);
    else
      error(expr->getSrcInfo(), "not a static unary expression");
  }
}

void StaticVisitor::visit(const IfExpr *expr) {
  std::tie(evaluated, value) = transform(expr->cond);
  // Note: both expressions must be evaluated at this time in order to capture
  // all
  //       unrealized variables (i.e. short-circuiting is not possible)
  auto i = transform(expr->eif);
  auto e = transform(expr->eelse);
  if (evaluated)
    std::tie(evaluated, value) = value ? i : e;
}

void StaticVisitor::visit(const BinaryExpr *expr) {
  std::tie(evaluated, value) = transform(expr->lexpr);
  bool evaluated2;
  int value2;
  std::tie(evaluated2, value2) = transform(expr->rexpr);
  evaluated &= evaluated2;
  if (!evaluated)
    return;
  if (expr->op == "<")
    value = value < value2;
  else if (expr->op == "<=")
    value = value <= value2;
  else if (expr->op == ">")
    value = value > value2;
  else if (expr->op == ">=")
    value = value >= value2;
  else if (expr->op == "==")
    value = value == value2;
  else if (expr->op == "!=")
    value = value != value2;
  else if (expr->op == "&&")
    value = value && value2;
  else if (expr->op == "||")
    value = value || value2;
  else if (expr->op == "+")
    value = value + value2;
  else if (expr->op == "-")
    value = value - value2;
  else if (expr->op == "*")
    value = value * value2;
  else if (expr->op == "//")
    value = value / value2;
  else if (expr->op == "%")
    value = value % value2;
  else
    error(expr->getSrcInfo(), "not a static binary expression");
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

void TypecheckVisitor::fixExprName(ExprPtr &e, const string &newName) {
  if (auto i = CAST(e, CallExpr)) // partial calls
    fixExprName(i->expr, newName);
  else if (auto i = CAST(e, IdExpr))
    i->value = newName;
  else if (auto i = CAST(e, DotExpr))
    i->member = newName;
  else {
    LOG("[fixExprName] can't fix {}", *e);
    assert(false);
  }
}

bool TypecheckVisitor::wrapOptional(TypePtr lt, ExprPtr &rhs) {
  auto lc = lt->getClass();
  auto rc = rhs->getType()->getClass();
  if (lc && lc->name == ".Optional" && rc && rc->name != ".Optional") {
    rhs = transform(
        Nx<CallExpr>(rhs.get(), Nx<IdExpr>(rhs.get(), ".Optional"), rhs->clone()));
    forceUnify(lc, rhs->getType());
    return true;
  }
  return false;
}

FuncTypePtr TypecheckVisitor::findBestCall(ClassTypePtr c, const string &member,
                                           const vector<pair<string, TypePtr>> &args,
                                           bool failOnMultiple, TypePtr retType) {
  auto m = ctx->findMethod(c->name, member);
  if (!m)
    return nullptr;

  if (m->size() == 1) // works
    return (*m)[0];

  // TODO: For now, overloaded functions are only possible in magic methods
  // Another assomption is that magic methods of interest have no default
  // arguments or reordered arguments...
  if (member.substr(0, 2) != "__" || member.substr(member.size() - 2) != "__")
    error("overloaded non-magic method...");
  for (auto &a : args)
    if (!a.first.empty())
      error("[todo] named magic call");

  vector<pair<int, int>> scores;
  for (int i = 0; i < m->size(); i++) {
    auto mt = dynamic_pointer_cast<FuncType>(
        ctx->instantiate(getSrcInfo(), (*m)[i], c, false));
    auto s = 0;
    if (mt->args.size() - 1 != args.size())
      continue;
    for (int j = 0; j < args.size(); j++) {
      Unification us;
      int u = args[j].second->unify(mt->args[j + 1], us);
      us.undo();
      if (u < 0) {
        s = -1;
        break;
      } else {
        s += u;
      }
    }
    if (retType) {
      Unification us;
      int u = retType->unify(mt->args[0], us);
      us.undo();
      s = u < 0 ? -1 : s + u;
    }
    if (s >= 0)
      scores.push_back({s, i});
  }
  if (!scores.size())
    return nullptr;
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
  return (*m)[scores[0].second];
}

vector<int> TypecheckVisitor::callFunc(types::TypePtr f, vector<CallExpr::Arg> &args,
                                       vector<CallExpr::Arg> &reorderedArgs,
                                       const vector<int> &availableArguments) {
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

  if (namedArgs.size() == 0 && reorderedArgs.size() == availableArguments.size() + 1 &&
      CAST(reorderedArgs.back().value, EllipsisExpr)) {
    isPartial = true;
    reorderedArgs.pop_back();
  } else if (reorderedArgs.size() + namedArgs.size() > availableArguments.size()) {
    error("too many arguments for {} (expected {}, got {})", f->toString(),
          availableArguments.size(), reorderedArgs.size() + namedArgs.size());
  }

  FunctionStmt *ast = nullptr;
  auto fc = f->getClass();
  auto &t_args = fc->args;
  if (auto ff = f->getFunc())
    ast = (FunctionStmt *)(ctx->cache->asts[ff->name].get());
  if (!ast && namedArgs.size())
    error("unexpected name '{}' (function pointers have argument "
          "names elided)",
          namedArgs.begin()->first);
  for (int i = 0, ra = reorderedArgs.size(); i < availableArguments.size(); i++) {
    if (i >= ra) {
      assert(ast);
      auto it = namedArgs.find(ast->args[availableArguments[i]].name);
      if (it != namedArgs.end()) {
        reorderedArgs.push_back({"", move(it->second)});
        namedArgs.erase(it);
      } else if (ast->args[i].deflt) {
        reorderedArgs.push_back(
            {"", transform(ast->args[availableArguments[i]].deflt)});
      } else {
        error("argument '{}' missing", ast->args[availableArguments[i]].name);
      }
    }
    if (CAST(reorderedArgs[i].value, EllipsisExpr))
      pending.push_back(availableArguments[i]);
    if (!wrapOptional(t_args[availableArguments[i] + 1], reorderedArgs[i].value))
      forceUnify(reorderedArgs[i].value, t_args[availableArguments[i] + 1]);
  }
  for (auto &i : namedArgs)
    error(i.second, "unknown argument {}", i.first);
  if (isPartial || pending.size())
    pending.push_back(args.size());
  return pending;
}

vector<types::Generic> TypecheckVisitor::parseGenerics(const vector<Param> &generics,
                                                       int level) {
  auto genericTypes = vector<types::Generic>();
  for (auto &g : generics) {
    assert(!g.name.empty());
    if (g.type && g.type->toString() != "(#id int)")
      error("only int generic types are allowed");
    auto tp = ctx->addUnbound(getSrcInfo(), level, true, bool(g.type));
    genericTypes.push_back(
        {g.name, tp->generalize(level), ctx->cache->unboundCount - 1});
    LOG7("[generic] {} -> {} {}", g.name, tp->toString(0), bool(g.type));
    ctx->add(TypecheckItem::Type, g.name, tp, false, true, bool(g.type));
  }
  return genericTypes;
}

types::TypePtr TypecheckVisitor::realizeFunc(types::TypePtr tt) {
  auto t = tt->getFunc();
  assert(t && t->canRealize());
  try {
    auto it = ctx->cache->realizations[t->name].find(t->realizeString());
    if (it != ctx->cache->realizations[t->name].end()) {
      forceUnify(t, it->second);
      return it->second;
    }

    LOG7("[realize] fn {} -> {}", t->name, t->realizeString());
    ctx->addBlock();
    ctx->bases.push_back({t});
    auto *ast = (FunctionStmt *)(ctx->cache->asts[t->name].get());
    int pi = 0;
    for (auto p = t->parent; p; pi++) {
      vector<Generic> *explicits = nullptr;
      auto po = p;
      if (auto y = p->getClass()) {
        explicits = &(y->explicits);
        p = y->parent;
      } else if (auto y = p->getFunc()) {
        explicits = &(y->explicits);
        p = y->parent;
      } else {
        assert(false);
      }
      for (auto &g : *explicits)
        if (auto s = g.type->getStatic())
          ctx->add(TypecheckItem::Type, g.name, s, false, false, true);
        else if (!g.name.empty())
          ctx->add(TypecheckItem::Type, g.name, g.type, true);
    }
    for (auto &g : t->explicits)
      if (auto s = g.type->getStatic())
        ctx->add(TypecheckItem::Type, g.name, s, false, false, true);
      else if (!g.name.empty())
        ctx->add(TypecheckItem::Type, g.name, g.type, true);
    // There is no AST linked to internal functions, so just ignore them
    bool isInternal = in(ast->attributes, "internal");
    isInternal |= ast->suite == nullptr;
    if (!isInternal)
      for (int i = 1; i < t->args.size(); i++) {
        assert(t->args[i] && !t->args[i]->hasUnbound());
        ctx->add(TypecheckItem::Var, ast->args[i - 1].name,
                 make_shared<LinkType>(t->args[i]));
      }

    // Need to populate funcRealization in advance to make recursive functions
    // viable
    ctx->cache->realizations[t->name][t->realizeString()] = t;
    ctx->cache->astTypes[t->realizeString()] = t;
    // ctx->getRealizations()->realizationLookup[t->realizeString()] = name;

    StmtPtr realized = nullptr;
    if (!isInternal) {
      realized = realizeBlock(ast->suite);
      forceUnify(t->args[0], ctx->bases.back().returnType ? ctx->bases.back().returnType
                                                          : ctx->findInternal("void"));
    }
    assert(t->args[0]->getClass() && t->args[0]->getClass()->canRealize());
    realizeType(t->args[0]->getClass());
    assert(ast->args.size() == t->args.size() - 1);
    vector<Param> args;
    for (auto &i : ast->args)
      args.push_back({i.name, nullptr, nullptr});

    ctx->cache->realizationAsts[t->realizeString()] =
        Nx<FunctionStmt>(ast, t->realizeString(), nullptr, vector<Param>(), move(args),
                         move(realized), ast->attributes);
    ctx->bases.pop_back();
    ctx->popBlock();
    return t;
  } catch (exc::ParserException &e) {
    e.trackRealize(fmt::format("{} (arguments {})", t->name, t->toString(1)),
                   getSrcInfo());
    throw;
  }
}

types::TypePtr TypecheckVisitor::realizeType(types::TypePtr tt) {
  auto t = tt->getClass();
  assert(t && t->canRealize());
  try {
    auto it = ctx->cache->realizations[t->name].find(t->realizeString());
    if (it != ctx->cache->realizations[t->name].end())
      return it->second;

    LOG7("[realize] ty {} -> {}", t->name, t->realizeString());
    for (auto &m : ctx->cache->classMembers[t->name]) {
      auto mt = ctx->instantiate(t->getSrcInfo(), m.second, t);
      LOG7("- member: {} -> {}: {}", m.first, m.second->toString(), mt->toString());
      assert(mt->getClass() && mt->getClass()->canRealize());
      ctx->cache->memberRealizations[t->realizeString()].push_back(
          {m.first, realizeType(mt->getClass())});
    }
    // ctx->getRealizations()->realizationLookup[rs] = t->name;
    ctx->cache->astTypes[t->realizeString()] = t;
    return ctx->cache->realizations[t->name][t->realizeString()] = t;
  } catch (exc::ParserException &e) {
    e.trackRealize(t->toString(), getSrcInfo());
    throw;
  }
}

StmtPtr TypecheckVisitor::realizeBlock(const StmtPtr &stmt, bool keepLast) {
  if (!stmt)
    return nullptr;
  StmtPtr result = nullptr;

  // We keep running typecheck transformations until there are no more unbound
  // types. It is assumed that the unbound count will decrease in each
  // iteration--- if not, the program cannot be type-checked.
  // TODO: this can be probably optimized one day...
  int minUnbound = ctx->cache->unboundCount;
  for (int iter = 0, prevSize = INT_MAX;; iter++) {
    ctx->addBlock();
    result = TypecheckVisitor(ctx).transform(result ? result : stmt);

    int newUnbounds = 0;
    std::set<types::TypePtr> newActiveUnbounds;
    for (auto i = ctx->activeUnbounds.begin(); i != ctx->activeUnbounds.end();) {
      auto l = (*i)->getLink();
      assert(l);
      if (l->kind == LinkType::Unbound) {
        newActiveUnbounds.insert(*i);
        if (l->id >= minUnbound)
          newUnbounds++;
      }
      ++i;
    }
    ctx->activeUnbounds = newActiveUnbounds;

    ctx->popBlock();
    if (ctx->activeUnbounds.empty() || !newUnbounds) {
      break;
    } else {
      if (newUnbounds >= prevSize) {
        TypePtr fu = nullptr;
        for (auto &ub : ctx->activeUnbounds)
          if (ub->getLink()->id >= minUnbound) {
            if (!fu)
              fu = ub;
            LOG7("[realizeBlock] dangling {} @ {}", ub->toString(), ub->getSrcInfo());
          }
        error(fu, "cannot resolve unbound variables");
      }
      prevSize = newUnbounds;
    }
    LOG7("===========================");
  }
  // Last pass; TODO: detect if it is needed...
  ctx->addBlock();
  LOG7("===========================");
  result = TypecheckVisitor(ctx).transform(result);
  if (!keepLast)
    ctx->popBlock();
  return result;
}

} // namespace ast
} // namespace seq
