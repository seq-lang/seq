/**
 * TODO : Redo error messages (right now they are awful)
 */

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
  TransformVisitor v(ctx, prependStmts);
  v.setSrcInfo(expr->getSrcInfo());
  expr->accept(v);
  if (v.resultExpr && v.resultExpr->getType() && v.resultExpr->getType()->getClass() &&
      v.resultExpr->getType()->getClass()->canRealize())
    realizeType(v.resultExpr->getType()->getClass());
  return move(v.resultExpr);
}

ExprPtr TypecheckVisitor::transformType(const ExprPtr &expr) {
  auto e = transform(expr.get(), true);
  if (e && !e->isType())
    error("expected type expression");
  e->setType(ctx->instantiate(expr->getSrcInfo(), e->getType()));
  return e;
}

/*************************************************************************************/

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
  assert(val);

  if (val->getStatic()) {
    auto s = val->getStatic()->getType()->getStatic();
    assert(s);
    resultExpr = transform(N<IntExpr>(s->getValue()));
    return;
  }

  TypePtr typ = val->getType();
  if (val->getClass())
    resultExpr->setType();
  else
    typ = ctx->instantiate(getSrcInfo(), val->getType());
  resultExpr = clone(expr);
  resultExpr->setType(forceUnify(resultExpr, typ));
  auto newName = patchIfRealizable(typ, val->getClass());
  if (!newName.empty())
    static_cast<IdExpr *>(resultExpr.get())->value = newName;
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
    resultExpr->setType(forceUnify(expr, ctx->findInternal("bool")));
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
    if (c && c->name == "Generator") {
      return c->explicits[0].type;
    } else
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
      auto r = realizeFunc(f->getFunc());
      if (isPartial)
        fixExprName(fe, r.fullName);
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
      items.push_back({l.op, transform(ce)});
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

void TypecheckVisitor::visit(const IndexExpr *expr) {
  ExprPtr e = transform(expr->expr, true);

  vector<TypePtr> generics;
  auto parseStatic = [&](const ExprPtr &i) {
    StaticVisitor sv(ctx);
    sv.transform(i.get());
    vector<types::Generic> v;
    for (auto &i : sv.captures)
      v.push_back(i.second);
    /// special case: generic static expressions
    if (auto ie = dynamic_cast<IdExpr *>(i.get())) {
      assert(v.size() == 1);
      generics.push_back(ctx->instantiate(getSrcInfo(), v[0].type));
    } else {
      vector<string> s;
      for (auto &i : sv.captures)
        s.push_back(i.first);
      // LOG7("static: {} -> {}", i->toString(), join(s, ", "));
      generics.push_back(make_shared<StaticType>(v, i->clone()));
    }
  };
  auto parseGeneric = [&](const ExprPtr &i) {
    try { // TODO: handle this better
      parseStatic(i);
    } catch (exc::ParserException &e) {
      LOG9("[index] failback, {}", e.what());
      auto ti = transform(i, true);
      if (ti->isType())
        generics.push_back(ctx->instantiate(getSrcInfo(), ti->getType()));
      else
        error(i, "expected a type or a static expression");
    }
  };

  // Type or function realization (e.g. dict[type1, type2])
  if (e->isType() || (e->getType()->getFunc())) {
    if (auto t = CAST(expr->index, TupleExpr))
      for (auto &i : t->items)
        parseGeneric(i);
    else
      parseGeneric(expr->index);
    auto g = ctx->instantiate(e->getSrcInfo(), e->getType())->getClass();
    if (g->explicits.size() != generics.size())
      error("expected {} generics, got {}", g->explicits.size(), generics.size());
    for (int i = 0; i < generics.size(); i++)
      /// Note: at this point, only single-variable static var expression (e.g.
      /// N) is allowed, so unify will work as expected.
      forceUnify(g->explicits[i].type, generics[i]);
    bool isType = e->isType();
    auto t = forceUnify(expr, g);
    auto newName = patchIfRealizable(t, isType);
    if (!newName.empty())
      fixExprName(e, newName);

    resultExpr = move(e); // will get replaced by identifier later on
    if (isType)
      resultExpr->markType();
    resultExpr->setType(t);
  } else if (auto c = e->getType()->getClass()) {
    auto getTupleIndex = [&](auto tuple, const auto &expr, const auto &index) {
      auto getInt = [](seq_int_t *o, const ExprPtr &e) {
        if (!e) {
          return true;
        }
        try {
          if (auto i = CAST(e, IntExpr)) {
            *o = std::stoll(i->value);
            return true;
          }
        } catch (std::out_of_range &) {
        }
        return false;
      };
      if (chop(tuple->name).substr(0, 6) != "Tuple.")
        return false;
      seq_int_t s = 0, e = tuple->args.size(), st = 1;
      if (getInt(&s, index)) {
        resultExpr = transform(N<TupleIndexExpr>(expr->clone(), translateIndex(s, e)));
        return true;
      } else if (auto i = CAST(index, SliceExpr)) {
        if (!getInt(&s, i->st) || !getInt(&e, i->ed) || !getInt(&st, i->step))
          return false;
        sliceAdjustIndices(tuple->args.size(), &s, &e, st);
        vector<ExprPtr> te;
        for (auto i = s; (st >= 0) ? (i < e) : (i >= e); i += st)
          te.push_back(N<TupleIndexExpr>(expr->clone(), i));
        resultExpr = transform(N<TupleExpr>(move(te)));
        return true;
      }
      return false;
    };
    if (!getTupleIndex(c, expr->expr, expr->index))
      resultExpr = transform(N<CallExpr>(N<DotExpr>(expr->expr->clone(), "__getitem__"),
                                         expr->index->clone()));
  } else {
    resultExpr = N<IndexExpr>(move(e), transform(expr->index));
    resultExpr->setType(expr->getType()
                            ? expr->getType()
                            : ctx->addUnbound(getSrcInfo(), ctx->getLevel()));
  }
}

void TypecheckVisitor::visit(const TupleIndexExpr *expr) {
  auto e = transform(expr->expr);
  auto c = e->getType()->getClass();
  assert(chop(c->name).substr(0, 6) == "Tuple.");
  if (expr->index < 0 || expr->index >= c->args.size())
    error("tuple index out of range (expected 0..{}, got {})", c->args.size() - 1,
          expr->index);
  resultExpr = N<TupleIndexExpr>(move(e), expr->index);
  resultExpr->setType(forceUnify(expr, c->args[expr->index]));
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
        fixExprName(ra.value, r.fullName);
      }
      // TODO: realize partials
    }

  if (c->canRealize() && c->getFunc()) {
    auto r = realizeFunc(c->getFunc());
    if (!isPartial)
      fixExprName(e, r.fullName);
  }
  TypePtr t = make_shared<LinkType>(t_args[0]);
  if (pending.size()) {
    pending.pop_back();
    string known(t_args.size() - 1, '1');
    for (auto p : pending)
      known[p] = '0';
    auto v = generatePartialStub(known);
    t = ctx->instantiateGeneric(getSrcInfo(), ctx->findInternal(v), {c});
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
    if (auto m = ctx->getRealizations()->findMethod(c->name, expr->member)) {
      if (m->size() > 1)
        error("ambigious partial expression"); /// TODO
      if (lhs->isType()) {
        auto name = (*m)[0]->name;
        auto val = processIdentifier(ctx, name);
        assert(val);
        auto t = ctx->instantiate(getSrcInfo(), (*m)[0], c);
        resultExpr = N<IdExpr>(name);
        resultExpr->setType(t);
        auto newName = patchIfRealizable(t, val->getClass());
        if (!newName.empty())
          static_cast<IdExpr *>(resultExpr.get())->value = newName;
        return;
      } else { // cast y.foo to CLS.foo(y, ...)
        auto f = (*m)[0];
        vector<ExprPtr> args;
        args.push_back(move(lhs));
        for (int i = 0; i < std::max(1, (int)f->args.size() - 2); i++)
          args.push_back(N<EllipsisExpr>());

        auto ast =
            dynamic_cast<FunctionStmt *>(ctx->getRealizations()->getAST(f->name).get());
        assert(ast);
        if (in(ast->attributes, "property"))
          args.pop_back();
        resultExpr = transform(N<CallExpr>(N<IdExpr>((*m)[0]->name), move(args)));
        return;
      }
    } else if (auto mm = ctx->getRealizations()->findMember(c->name, expr->member)) {
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
                                               ctx->findInternal("Ptr"), {t})));
}

void TypecheckVisitor::visit(const YieldExpr *expr) {
  resultExpr = N<YieldExpr>();
  if (!ctx->getLevel() || !ctx->bases.back().parent->getFunc())
    error("(yield) cannot be used outside of functions");
  auto t = ctx->instantiateGeneric(getSrcInfo(), ctx->findInternal("Generator"),
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

void TransformVisitor::visit(const SuiteStmt *stmt) {
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

void TransformVisitor::visit(const ExprStmt *stmt) {
  resultStmt = N<ExprStmt>(transform(stmt->expr));
}

void TransformVisitor::visit(const AssignStmt *stmt) {
  auto l = CAST(stmt->lhs, IdExpr);
  assert(l);

  auto rhs = transform(stmt->rhs);
  auto typExpr = transformType(stmt->type);
  if (typExpr && typExpr->getType()->getClass()) {
    auto typ = ctx->instantiate(getSrcInfo(), typExpr->getType());
    if (!wrapOptional(typ, rhs))
      forceUnify(typ, rhs->getType());
  }

  if (rhs->isType())
    ctx->addType(l->value, rhs->getType());
  else if (dynamic_pointer_cast<FuncType>(rhs->getType()))
    ctx->addFunc(l->value, rhs->getType());
  else
    ctx->addVar(l->value, rhs->getType());
  resultStmt = N<AssignStmt>(clone(l), move(rhs), move(typExpr));
}

void TransformVisitor::visit(const UpdateStmt *stmt) {
  auto l = transform(stmt->lhs);
  auto r = transform(stmt->rhs);
  forceUnify(l->getType(), r->getType());
  if (!wrapOptional(l->getType(), r))
    s->lhs->setType(forceUnify(r.get(), l->getType()));
  resultStmt = N<UpdateStmt>(move(l), move(r));
}

void TransformVisitor::visit(const AssignMemberStmt *stmt) {
  auto lh = transform(stmt->lhs), rh = transform(stmt->rhs);
  auto c = lh->getType()->getClass();
  if (c && c->isRecord())
    error("records are read-only ^ {} , {}", c->toString(), lh->toString());
  auto mm = ctx->getRealizations()->findMember(c->name, stmt->member);
  forceUnify(ctx->instantiate(getSrcInfo(), mm, c), rh->getType());
  resultStmt = N<AssignMemberStmt>(move(lh), stmt->member, move(rh));
}

void TransformVisitor::visit(const DelStmt *stmt) {
  auto expr = CAST(stmt->expr, IdExpr);
  assert(expr);
  ctx->remove(expr->value);
  resultStmt = N<DelStmt>(transform(expr));
}

void TransformVisitor::visit(const ReturnStmt *stmt) {
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

void TransformVisitor::visit(const YieldStmt *stmt) {
  types::TypePtr t = nullptr;
  if (stmt->expr) {
    auto e = transform(stmt->expr);
    t = ctx->instantiateGeneric(e->getSrcInfo(), ctx->findInternal("Generator"),
                                {e->getType()});
    resultStmt = N<YieldStmt>(move(e));
  } else {
    t = ctx->instantiateGeneric(stmt->getSrcInfo(), ctx->findInternal("Generator"),
                                {ctx->findInternal("void")});
    resultStmt = N<YieldStmt>(nullptr);
  }
  auto &base = ctx->bases.back();
  if (base.returnType)
    forceUnify(t, base.returnType);
  else
    base.returnType = t;
}

void TransformVisitor::visit(const AssertStmt *stmt) {
  resultStmt = N<AssertStmt>(transform(stmt->expr));
}

void TransformVisitor::visit(const WhileStmt *stmt) {
  resultStmt = N<WhileStmt>(transform(stmt->cond), transform(stmt->suite));
}

void TransformVisitor::visit(const ForStmt *stmt) {
  auto iter = transform(stmt->iter);
  TypePtr varType = nullptr;
  varType = ctx->addUnbound(stmt->var->getSrcInfo(), ctx->getLevel());
  if (!iter->getType()->getUnbound()) {
    auto iterType = iter->getType()->getClass();
    if (!iterType || iterType->name != "Generator")
      error(iter, "expected a generator");
    forceUnify(varType, iterType->explicits[0].type);
  }
  ctx->addBlock();
  auto i = CAST(stmt->var, IdExpr);
  assert(i);
  string varName = i->value;
  ctx->addVar(varName, varType);
  resultStmt = N<ForStmt>(transform(stmt->var), move(iter), transform(stmt->suite));
  ctx->popBlock();
}

void TransformVisitor::visit(const IfStmt *stmt) {
  vector<IfStmt::If> ifs;
  for (auto &i : stmt->ifs)
    ifs.push_back({transform(i.cond), transform(i.suite)});
  resultStmt = N<IfStmt>(move(ifs));
}

void TransformVisitor::visit(const MatchStmt *stmt) {
  auto w = transform(stmt->what);
  ctx->setMatchType(w->getType());
  vector<PatternPtr> patterns;
  vector<StmtPtr> cases;
  for (auto ci = 0; ci < stmt->cases.size(); ci++) {
    ctx->addBlock();
    if (auto p = CAST(stmt->patterns[ci], BoundPattern)) {
      auto boundPat = transform(p->pattern);
      ctx->addVar(p->var, boundPat->getType());
      patterns.push_back(move(boundPat));
      cases.push_back(transform(stmt->cases[ci]));
    } else {
      patterns.push_back(transform(stmt->patterns[ci]));
      cases.push_back(transform(stmt->cases[ci]));
    }
    ctx->popBlock();
  }
  ctx->setMatchType(nullptr);
  resultStmt = N<MatchStmt>(move(w), move(patterns), move(cases));
}

void TransformVisitor::visit(const TryStmt *stmt) {
  vector<TryStmt::Catch> catches;
  auto suite = transform(stmt->suite);
  for (auto &c : stmt->catches) {
    ctx->addBlock();
    auto exc = transformType(c.exc);
    if (c.var != "")
      ctx->addVar(c.var, exc->getType());
    catches.push_back({c.var, move(exc), transform(c.suite)});
    ctx->popBlock();
  }
  resultStmt = N<TryStmt>(move(suite), move(catches), transform(stmt->finally));
}

void TransformVisitor::visit(const ThrowStmt *stmt) {
  resultStmt = N<ThrowStmt>(transform(stmt->expr));
}

void TransformVisitor::visit(const FunctionStmt *stmt) {
  resultStmt = N<FunctionStmt>(stmt->name, nullptr, vector<Param>(), vector<Param>(),
                               nullptr, stmt->attributes);
  bool isClassMember = ctx->getLevel() && !ctx->bases.back().parent->getFunc();
  auto it = ctx->cache->astTypes.find(stmt->name);
  if (it != ctx->cache->astTypes.end()) {
    if (!isClassMember)
      ctx->addFunc(stmt->name, it->second);
    return;
  }

  LOG7("[stmt] adding func {} (base: {})", stmt->name, ctx->getBase());

  auto t = make_shared<FuncType>(
      stmt->name, ctx->findInternal(format("Function.{}", stmt->args.size())));
  ctx->bases.push_back({t});
  t->explicits =
      parseGenerics(stmt->generics, ctx->getLevel() - 1); // generics are level down
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
    ctx->addVar(a.name, t->args.back());
  }
  ctx->bases.pop_back();

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
  LOG7("[stmt] added func {}: {} (@{}; ref={})", stmt->name, t->toString(),
       t->parent ? t->parent->toString() : "-", ref);
  ctx->cache->astTypes[stmt->name] = t;

  if ((in(stmt->attributes, "builtin") || in(stmt->attributes, ".c")) &&
      !t->canRealize())
    error("builtins and external functions must be realizable");
  // Class members are realized after the class is sealed to prevent premature
  // unification of class generics
  if (!isClassMember && t->canRealize())
    realizeFunc(ctx->instantiate(getSrcInfo(), t)->getFunc());
}

void TransformVisitor::visit(const ClassStmt *stmt) {
  resultStmt = N<ClassStmt>(stmt->isRecord, stmt->name, vector<Param>(),
                            vector<Param>(), N<SuiteStmt>(), stmt->attributes);
  auto it = ctx->cache->astTypes.find(stmt->name);
  if (it != ctx->cache->astTypes.end()) {
    ctx->addType(stmt->name, it->second);
    return;
  }

  LOG7("[stmt] adding type {} (base: {})", stmt->name, ctx->getBase());
  vector<StmtPtr> stmts;
  stmts.push_back(move(resultStmt));

  auto ct = make_shared<ClassType>(
      stmt->name, stmt->isRecord, vector<TypePtr>(), vector<Generic>(),
      ctx->getLevel() ? ctx->bases.back().parent : nullptr);
  ct->setSrcInfo(stmt->getSrcInfo());
  auto ctxi =
      make_shared<TypeItem::Class>(ct, false, ctx->getFilename(), ctx->getBase(), true);
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

/*******************************/

StaticVisitor::StaticVisitor(std::shared_ptr<TypeContext> ctx,
                             const std::unordered_map<std::string, types::Generic> *m)
    : ctx(ctx), map(m), evaluated(false), value(0) {}

pair<bool, int> StaticVisitor::transform(const Expr *e) {
  StaticVisitor v(ctx, map);
  e->accept(v);
  for (auto &i : v.captures)
    captures.insert({i.first, i.second});
  return {v.evaluated, v.evaluated ? v.value : -1};
}

void StaticVisitor::visit(const IdExpr *expr) {
  types::TypePtr t = nullptr;
  if (ctx) {
    auto val = ctx->find(expr->value);
    if (!val)
      error(expr->getSrcInfo(), "identifier '{}' not found", expr->value);
    if (!val->getStatic())
      error(expr->getSrcInfo(), "identifier '{}' is not a static expression",
            expr->value);
    t = val->getStatic()->getType()->follow();
  } else {
    assert(map);
    auto val = map->find(expr->value);
    if (val == map->end())
      error(expr->getSrcInfo(), "identifier '{}' not found", expr->value);
    t = val->second.type->follow();
  }
  if (t->getLink()) {
    evaluated = false;
    captures[expr->value] = {expr->value, t, t->getLink()->id};
  } else {
    assert(t->getStatic() && t->getStatic()->explicits.size() <= 1);
    captures[expr->value] = {
        expr->value, t,
        t->getStatic()->explicits.size() ? t->getStatic()->explicits[0].id : 0};
    evaluated = t->canRealize();
    if (evaluated)
      value = t->getStatic()->getValue();
  }
}

void StaticVisitor::visit(const IntExpr *expr) {
  if (expr->suffix.size())
    error(expr->getSrcInfo(), "not a static expression");
  try {
    value = std::stoull(expr->value, nullptr, 0);
    evaluated = true;
  } catch (std::out_of_range &) {
    error(expr->getSrcInfo(), "integer {} out of range", expr->value);
  }
}

void StaticVisitor::visit(const UnaryExpr *expr) {
  std::tie(evaluated, value) = transform(expr->expr.get());
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
  std::tie(evaluated, value) = transform(expr->cond.get());
  // Note: both expressions must be evaluated at this time in order to capture
  // all
  //       unrealized variables (i.e. short-circuiting is not possible)
  auto i = transform(expr->eif.get());
  auto e = transform(expr->eelse.get());
  if (evaluated)
    std::tie(evaluated, value) = value ? i : e;
}

void StaticVisitor::visit(const BinaryExpr *expr) {
  std::tie(evaluated, value) = transform(expr->lexpr.get());
  bool evaluated2;
  int value2;
  std::tie(evaluated2, value2) = transform(expr->rexpr.get());
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

} // namespace ast
} // namespace seq
