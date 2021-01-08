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

ExprPtr TypecheckVisitor::transform(const ExprPtr &expr_) {
  auto &expr = const_cast<ExprPtr &>(expr_);
  expr = transform(expr, false);
  return move(expr);
}

ExprPtr TypecheckVisitor::transform(ExprPtr &expr, bool allowTypes, bool allowVoid) {
  if (!expr)
    return nullptr;
  auto typ = expr->type;
  if (!expr->done) {
    TypecheckVisitor v(ctx, prependStmts);
    v.allowVoidExpr = allowVoid;
    v.setSrcInfo(expr->getSrcInfo());
    expr->accept(v);
    if (v.resultExpr)
      expr = move(v.resultExpr);
    seqassert(expr->type, "type not set for {}", expr->toString());
    typ |= expr->type;
  }
  realizeType(typ->getClass());
  if (!expr->isType() && !allowVoid &&
      (expr->type->is("void") || expr->type->is("T.None")))
    error("expression with void type");
  return move(expr);
}

ExprPtr TypecheckVisitor::transformType(ExprPtr &expr) {
  expr = transform(const_cast<ExprPtr &>(expr), true);
  if (expr) {
    if (!expr->isType())
      error("expected type expression");
    auto t = ctx->instantiate(expr->getSrcInfo(), expr->getType());
    LOG_TYPECHECK("[inst] {} -> {}", expr->toString(), t->toString());
    expr->setType(t);
  }
  return move(expr);
}

void TypecheckVisitor::defaultVisit(Expr *e) {
  seqassert(false, "unexpected AST node {}", e->toString());
}

/**************************************************************************************/

void TypecheckVisitor::visit(BoolExpr *expr) {
  expr->type |= ctx->findInternal("bool");
  expr->done = true;
  expr->isStaticExpr = true;
  expr->staticEvaluation = {true, int(expr->value)};
}

void TypecheckVisitor::visit(IntExpr *expr) {
  expr->type |= ctx->findInternal("int");
  expr->done = true;
  expr->staticEvaluation = {true, expr->intValue};
}

void TypecheckVisitor::visit(FloatExpr *expr) {
  expr->type |= ctx->findInternal("float");
  expr->done = true;
}

void TypecheckVisitor::visit(StringExpr *expr) {
  expr->type |= ctx->findInternal("str");
  expr->done = true;
}

void TypecheckVisitor::visit(IdExpr *expr) {
  auto val = ctx->find(expr->value);
  seqassert(val, "cannot find IdExpr '{}'", expr->value);
  if (val->isStatic()) {
    // Evaluate the static expression.
    seqassert(val->type->getStatic(), "{} does not have static type", expr->value);
    expr->staticEvaluation = {
        true, val->type->getStatic()->evaluate(val->type->getStatic().get())};
    resultExpr = transform(N<IntExpr>(expr->staticEvaluation.second));
    return;
  }
  if (val->isType())
    expr->markType();
  auto t = ctx->instantiate(getSrcInfo(), val->type);
  LOG_TYPECHECK("[inst] {} -> {}", expr->toString(), t->toString());
  expr->type |= t;

  /// Check if we can realize the type.
  if (getRealizedType(expr->type) &&
      (val->kind == TypecheckItem::Type || val->kind == TypecheckItem::Func))
    expr->value = expr->type->realizeString();
  expr->done = realizeType(expr->type) != nullptr;
}

void TypecheckVisitor::visit(IfExpr *expr) {
  expr->cond = transform(expr->cond);
  expr->ifexpr = transform(expr->ifexpr, false, allowVoidExpr);
  expr->elsexpr = transform(expr->elsexpr, false, allowVoidExpr);

  if (expr->isStaticExpr) {
    if (expr->cond->staticEvaluation.first && expr->ifexpr->staticEvaluation.first &&
        expr->elsexpr->staticEvaluation.first) {
      resultExpr = transform(N<IntExpr>(expr->cond->staticEvaluation.second
                                            ? expr->ifexpr->staticEvaluation.second
                                            : expr->elsexpr->staticEvaluation.second));
    }
  } else {
    wrapOptionalIfNeeded(expr->ifexpr->getType(), expr->elsexpr);
    wrapOptionalIfNeeded(expr->elsexpr->getType(), expr->ifexpr);
    expr->type |= expr->ifexpr->getType();
    expr->type |= expr->elsexpr->getType();
    expr->ifexpr = transform(expr->ifexpr);
    expr->elsexpr = transform(expr->elsexpr);
    expr->done = expr->cond->done && expr->ifexpr->done && expr->elsexpr->done;
  }
}

void TypecheckVisitor::visit(UnaryExpr *expr) {
  seqassert(expr->isStaticExpr && (expr->op == "-" || expr->op == "!"),
            "non-static unary expression");
  // Evaluate a static expression.
  expr->expr = transform(expr->expr);
  expr->type |= ctx->findInternal("int");
  if (expr->expr->staticEvaluation.first) {
    int value = expr->expr->staticEvaluation.second;
    if (expr->op == "-")
      value = -value;
    else // if (expr->op == "!")
      value = !bool(value);
    if (expr->op == "!")
      resultExpr = transform(N<BoolExpr>(value));
    else
      resultExpr = transform(N<IntExpr>(value));
  }
}

void TypecheckVisitor::visit(BinaryExpr *expr) {
  if (expr->isStaticExpr) {
    // Evaluate a static expression.
    expr->lexpr = transform(expr->lexpr);
    expr->rexpr = transform(expr->rexpr);
    expr->type |= ctx->findInternal("int");
    if (expr->lexpr->staticEvaluation.first && expr->rexpr->staticEvaluation.first) {
      int lvalue = expr->lexpr->staticEvaluation.second;
      int rvalue = expr->rexpr->staticEvaluation.second;
      if (expr->op == "<")
        lvalue = lvalue < rvalue;
      else if (expr->op == "<=")
        lvalue = lvalue <= rvalue;
      else if (expr->op == ">")
        lvalue = lvalue > rvalue;
      else if (expr->op == ">=")
        lvalue = lvalue >= rvalue;
      else if (expr->op == "==")
        lvalue = lvalue == rvalue;
      else if (expr->op == "!=")
        lvalue = lvalue != rvalue;
      else if (expr->op == "&&")
        lvalue = lvalue && rvalue;
      else if (expr->op == "||")
        lvalue = lvalue || rvalue;
      else if (expr->op == "+")
        lvalue = lvalue + rvalue;
      else if (expr->op == "-")
        lvalue = lvalue - rvalue;
      else if (expr->op == "*")
        lvalue = lvalue * rvalue;
      else if (expr->op == "//") {
        if (!rvalue)
          error("static division by zero");
        lvalue = lvalue / rvalue;
      } else if (expr->op == "%") {
        if (!rvalue)
          error("static division by zero");
        lvalue = lvalue % rvalue;
      } else {
        seqassert(false, "unknown static operator {}", expr->op);
      }
      if (in(set<string>{"==", "!=", "<", "<=", ">", ">=", "&&", "||"}, expr->op))
        resultExpr = transform(N<BoolExpr>(bool(lvalue)));
      else
        resultExpr = transform(N<IntExpr>(lvalue));
    }
  } else {
    resultExpr = transformBinary(expr);
  }
}

void TypecheckVisitor::visit(PipeExpr *expr) {
  // Returns T if t is of type Generator[T].
  auto getIterableType = [&](TypePtr t) {
    if (t->is("Generator"))
      return t->getClass()->explicits[0].type;
    return t;
  };
  // List of output types (for a|>b|>c, this list is type(a), type(a|>b), type(a|>b|>c).
  // These types are raw types (i.e. generator types are preserved).
  expr->inTypes.clear();
  expr->items[0].expr = transform(expr->items[0].expr);

  // The input type to the next stage.
  TypePtr inType = expr->items[0].expr->getType();
  expr->inTypes.push_back(inType);
  inType = getIterableType(inType);
  expr->done = expr->items[0].expr->done;
  for (int i = 1; i < expr->items.size(); i++) {
    ExprPtr prepend = nullptr; // An optional preceding stage
                               // (e.g. prepend  (|> unwrap |>) if an optional argument
                               //  needs unpacking).
    int inTypePos = -1;

    // Get the stage expression (take heed of StmtExpr!):
    auto ec = &expr->items[i].expr; // This is a pointer to a CallExprPtr
    while ((*ec)->getStmtExpr())
      ec = &const_cast<StmtExpr *>((*ec)->getStmtExpr())->expr;
    if (auto ecc = const_cast<CallExpr *>((*ec)->getCall())) {
      // Find the input argument position (a position of ... in the argument list):
      for (int ia = 0; ia < ecc->args.size(); ia++)
        if (auto ee = ecc->args[ia].value->getEllipsis()) {
          if (inTypePos == -1) {
            const_cast<EllipsisExpr *>(ee)->isPipeArg = true;
            inTypePos = ia;
            break;
          }
        }
      // If there is no ... in the argument list, use the first argument as the input
      // argument and add an ellipsis there
      if (inTypePos == -1) {
        ecc->args.insert(ecc->args.begin(), {"", N<EllipsisExpr>(true)});
        inTypePos = 0;
      }
    } else {
      // If this is not a CallExpr, make it a call expression with a single input
      // argument:
      expr->items[i].expr =
          N<CallExpr>(move(expr->items[i].expr), N<EllipsisExpr>(true));
      ec = &expr->items[i].expr;
      inTypePos = 0;
    }

    if (auto nn = transformCall((CallExpr *)(ec->get()), inType, &prepend))
      *ec = move(nn);
    if (prepend) { // Prepend the stage and rewind the loop (yes, the current
                   // expression will get parsed twice).
      expr->items.insert(expr->items.begin() + i, {"|>", move(prepend)});
      i--;
      continue;
    }
    if ((*ec)->type)
      expr->items[i].expr->type |= (*ec)->type;
    expr->items[i].expr = move(*ec);
    inType = expr->items[i].expr->getType();
    expr->inTypes.push_back(inType);
    // Do not extract the generator type in the last stage of a pipeline.
    if (i < expr->items.size() - 1)
      inType = getIterableType(inType);
    expr->done &= expr->items[i].expr->done;
  }
  expr->type |= inType;
}

void TypecheckVisitor::visit(InstantiateExpr *expr) {
  expr->typeExpr = transform(expr->typeExpr, true);
  auto typ = ctx->instantiate(expr->typeExpr->getSrcInfo(), expr->typeExpr->getType());
  LOG_TYPECHECK("[inst] {} -> {}", expr->typeExpr->toString(), typ->toString());
  seqassert(typ->getFunc() || typ->getClass(), "unknown type");
  auto &generics =
      typ->getFunc() ? typ->getFunc()->explicits : typ->getClass()->explicits;
  if (expr->typeParams.size() != generics.size())
    error("expected {} generics", generics.size());

  for (int i = 0; i < expr->typeParams.size(); i++) {
    if (expr->typeParams[i]->isStaticExpr) {
      if (auto ei = expr->typeParams[i]->getId()) {
        // Case 1: Generic static (e.g. N in a [N:int] scope).
        auto val = ctx->find(ei->value);
        seqassert(val && val->isStatic(), "invalid static expression");
        auto t = ctx->instantiate(getSrcInfo(), val->type);
        LOG_TYPECHECK("[inst] {} -> {}", expr->typeParams[i]->toString(),
                      t->toString());
        generics[i].type |= t;
      } else {
        // Case 2: Static expression (e.g. 32 or N+5).
        // Get the dependent types and create the underlying StaticType.
        unordered_set<string> seen;
        vector<Generic> staticGenerics;
        std::function<void(Expr *)> findGenerics = [&](Expr *e) -> void {
          if (auto ei = e->getId()) {
            if (!in(seen, ei->value)) {
              auto val = ctx->find(ei->value);
              seqassert(val && val->isStatic(), "invalid static expression");
              auto genTyp = val->type->follow();
              staticGenerics.emplace_back(Generic{
                  ei->value, genTyp,
                  genTyp->getLink() ? genTyp->getLink()->id
                                    : genTyp->getStatic()->explicits.empty()
                                          ? 0
                                          : genTyp->getStatic()->explicits[0].id});
              seen.insert(ei->value);
            }
          } else if (auto eu = e->getUnary()) {
            findGenerics(eu->expr.get());
          } else if (auto eb = e->getBinary()) {
            findGenerics(eb->lexpr.get());
            findGenerics(eb->rexpr.get());
          } else if (auto ef = e->getIf()) {
            findGenerics(ef->cond.get());
            findGenerics(ef->ifexpr.get());
            findGenerics(ef->elsexpr.get());
          }
        };
        findGenerics(expr->typeParams[i].get());
        auto nctx = ctx; // To capture ctx without capturing this...
        generics[i].type |= make_shared<StaticType>(
            staticGenerics, expr->typeParams[i]->clone(), [nctx](const StaticType *t) {
              nctx->addBlock();
              for (auto &g : t->explicits)
                nctx->add(TypecheckItem::Type, g.name, g.type, true);
              auto en = TypecheckVisitor(nctx).transform(t->expr->clone());
              seqassert(en->isStaticExpr && en->staticEvaluation.first,
                        "{} cannot be evaluated", en->toString());
              nctx->popBlock();
              return en->staticEvaluation.second;
            });
      }
    } else {
      seqassert(expr->typeParams[i]->isType(), "not a type: {}",
                expr->typeParams[i]->toString());
      expr->typeParams[i] = transform(expr->typeParams[i], true);
      auto t = ctx->instantiate(getSrcInfo(), expr->typeParams[i]->getType());
      LOG_TYPECHECK("[inst] {} -> {}", expr->typeParams[i]->toString(), t->toString());
      generics[i].type |= t;
    }
  }
  expr->type |= typ;
  if (expr->typeExpr->isType())
    expr->markType();

  // If this is realizable, use the realized name (e.g. use Id("Ptr[byte]") instead of
  // Instantiate(Ptr, {byte})).
  if (getRealizedType(expr->type)) {
    resultExpr = N<IdExpr>(expr->type->realizeString());
    resultExpr->type |= typ;
    resultExpr->done = true;
    if (expr->typeExpr->isType())
      resultExpr->markType();
  }
}

void TypecheckVisitor::visit(SliceExpr *expr) {
  ExprPtr none = N<CallExpr>(N<DotExpr>(N<IdExpr>("Optional"), "__new__"));
  resultExpr = transform(N<CallExpr>(N<IdExpr>("Slice"),
                                     expr->start ? move(expr->start) : clone(none),
                                     expr->stop ? move(expr->stop) : clone(none),
                                     expr->step ? move(expr->step) : clone(none)));
}

void TypecheckVisitor::visit(IndexExpr *expr) {
  expr->expr = transform(expr->expr, true);
  auto typ = expr->expr->getType();
  if (typ->getFunc()) {
    // Case 1: function instantiation
    vector<ExprPtr> it;
    if (auto et = const_cast<TupleExpr *>(expr->index->getTuple()))
      for (auto &i : et->items)
        it.push_back(move(i));
    else
      it.push_back(move(expr->index));
    resultExpr = transform(N<InstantiateExpr>(move(expr->expr), move(it)));
  } else if (auto c = typ->getClass()) {
    // Case 2: check if this is a static tuple access...
    resultExpr = transformStaticTupleIndex(c.get(), expr->expr, expr->index);
    if (!resultExpr)
      // Case 3: ... and if not, just call __getitem__.
      resultExpr = transform(
          N<CallExpr>(N<DotExpr>(move(expr->expr), "__getitem__"), move(expr->index)));
  } else {
    // Case 4: type is still unknown.
    expr->index = transform(expr->index);
    expr->type |= ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel);
  }
}

void TypecheckVisitor::visit(DotExpr *expr) { resultExpr = transformDot(expr); }

void TypecheckVisitor::visit(CallExpr *expr) { resultExpr = transformCall(expr); }

void TypecheckVisitor::visit(StackAllocExpr *expr) {
  expr->typeExpr = transformType(expr->typeExpr);
  expr->expr = transform(expr->expr);
  auto t = ctx->instantiateGeneric(expr->getSrcInfo(), ctx->findInternal("Array"),
                                   {expr->typeExpr->type});
  LOG_TYPECHECK("[inst] {} -> {}", expr->toString(), t->toString());
  expr->type |= t;
  // Realize the Array[T] type of possible.
  if (getRealizedType(expr->type))
    expr->done = expr->expr->done;
}

void TypecheckVisitor::visit(EllipsisExpr *expr) {
  expr->type |= ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel);
}

void TypecheckVisitor::visit(TypeOfExpr *expr) {
  expr->expr = transform(expr->expr);
  expr->type |= expr->expr->type;

  if (getRealizedType(expr->type)) {
    resultExpr = N<IdExpr>(expr->type->realizeString());
    resultExpr->type |= expr->type;
    resultExpr->done = true;
    resultExpr->markType();
  }
}

void TypecheckVisitor::visit(PtrExpr *expr) {
  expr->expr = transform(expr->expr);
  auto t = ctx->instantiateGeneric(expr->getSrcInfo(), ctx->findInternal("Ptr"),
                                   {expr->expr->type});
  LOG_TYPECHECK("[inst] {} -> {}", expr->toString(), t->toString());
  expr->type |= t;
  expr->done = expr->expr->done;
}

void TypecheckVisitor::visit(YieldExpr *expr) {
  seqassert(!ctx->bases.empty(), "yield outside of a function");
  auto typ =
      ctx->instantiateGeneric(getSrcInfo(), ctx->findInternal("Generator"),
                              {ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel)});
  LOG_TYPECHECK("[inst] {} -> {}", expr->toString(), typ->toString());
  ctx->bases.back().returnType |= typ;
  expr->type |= typ->getClass()->explicits[0].type;
  expr->done = realizeType(expr->type) != nullptr;
}

void TypecheckVisitor::visit(StmtExpr *expr) {
  expr->done = true;
  for (auto &s : expr->stmts) {
    s = transform(s);
    expr->done &= s->done;
  }
  expr->expr = transform(expr->expr, false, allowVoidExpr);
  expr->type |= expr->expr->type;
  expr->done &= expr->expr->done;
}

/**************************************************************************************/

TypePtr TypecheckVisitor::getRealizedType(TypePtr &typ) {
  if (typ->getFunc()) {
    if (auto tf = realizeFunc(typ->getFunc()))
      return typ |= tf;
  } else {
    if (auto tt = realizeType(typ->getClass()))
      return typ |= tt;
  }
  return nullptr;
}

void TypecheckVisitor::wrapOptionalIfNeeded(const TypePtr &targetType, ExprPtr &e) {
  if (!targetType)
    return;
  auto t1 = targetType->getClass();
  auto t2 = e->getType()->getClass();
  if (t1 && t2 && t1->name == "Optional" && t1->name != t2->name)
    e = transform(N<CallExpr>(N<IdExpr>("Optional"), move(e)));
}

ExprPtr TypecheckVisitor::transformBinary(BinaryExpr *expr, bool isAtomic,
                                          bool *noReturn) {
  // Table of supported binary operations and the corresponding magic methods.
  auto magics = unordered_map<string, string>{
      {"+", "add"},     {"-", "sub"},    {"*", "mul"}, {"**", "pow"}, {"/", "truediv"},
      {"//", "div"},    {"@", "matmul"}, {"%", "mod"}, {"<", "lt"},   {"<=", "le"},
      {">", "gt"},      {">=", "ge"},    {"==", "eq"}, {"!=", "ne"},  {"<<", "lshift"},
      {">>", "rshift"}, {"&", "and"},    {"|", "or"},  {"^", "xor"},
  };
  if (noReturn)
    *noReturn = false;

  expr->lexpr = transform(expr->lexpr);
  expr->rexpr = expr->rexpr->getNone() ? move(expr->rexpr) : transform(expr->rexpr);

  if (expr->lexpr->getType()->getUnbound() ||
      (expr->op != "is" && expr->rexpr->getType()->getUnbound())) {
    // Case 1: If operand types are unknown, continue later (no transformation).
    // Mark the type as unbound.
    expr->type |= ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel);
    return nullptr;
  }

  // Check if this is a "a is None" expression. If so, ...
  if (expr->op == "is" && expr->rexpr->getNone()) {
    if (expr->rexpr->getNone()) {
      if (expr->lexpr->getType()->getClass()->name != "Optional")
        // ... return False if lhs is not an Optional...
        return transform(N<BoolExpr>(false));
      else
        // ... or return lhs.__bool__.__invert__()
        return transform(N<CallExpr>(N<DotExpr>(
            N<CallExpr>(N<DotExpr>(move(expr->lexpr), "__bool__")), "__invert__")));
    }
  }

  // Check the type equality (operand types and __raw__ pointers must match).
  // TODO: more special cases needed (Optional[T] == Optonal[U] if both are None)
  if (expr->op == "is") {
    auto lc = realizeType(expr->lexpr->getType());
    auto rc = realizeType(expr->rexpr->getType());
    if (!lc || !rc) {
      // We still do not know the exact types...
      expr->type |= ctx->findInternal("bool");
      return nullptr;
    } else if (!lc->getClass()->isRecord() && !rc->getClass()->isRecord()) {
      return transform(
          N<BinaryExpr>(N<CallExpr>(N<DotExpr>(move(expr->lexpr), "__raw__")),
                        "==", N<CallExpr>(N<DotExpr>(move(expr->rexpr), "__raw__"))));
    } else if (lc->getClass()->name == "Optional") {
      return transform(N<CallExpr>(N<DotExpr>(move(expr->lexpr), "__is_optional__"),
                                   move(expr->rexpr)));
    } else if (rc->getClass()->name == "Optional") {
      return transform(N<CallExpr>(N<DotExpr>(move(expr->rexpr), "__is_optional__"),
                                   move(expr->lexpr)));
    } else if (lc->realizeString() != rc->realizeString()) {
      return transform(N<BoolExpr>(false));
    } else {
      return transform(N<BinaryExpr>(move(expr->lexpr), "==", move(expr->rexpr)));
    }
  }

  // Transform a binary expression to a magic method call.
  auto mi = magics.find(expr->op);
  if (mi == magics.end())
    error("invalid binary operator '{}'", expr->op);
  auto magic = mi->second;

  auto lt = expr->lexpr->getType()->getClass();
  auto rt = expr->rexpr->getType()->getClass();
  seqassert(lt && rt, "lhs and rhs types not known");

  FuncTypePtr method = nullptr;
  // Check if lt.__atomic_op__(Ptr[lt], rt) exists.
  if (isAtomic) {
    auto ptrlt = ctx->instantiateGeneric(getSrcInfo(), ctx->findInternal("Ptr"), {lt});
    LOG_TYPECHECK("[inst] {} -> {}", expr->lexpr->toString(), ptrlt->toString());
    method = findBestMethod(lt.get(), format("__atomic_{}__", magic),
                            {{"", ptrlt}, {"", rt}});
    if (method) {
      expr->lexpr = N<PtrExpr>(move(expr->lexpr));
      if (noReturn)
        *noReturn = true;
    }
  }
  // Check if lt.__iop__(lt, rt) exists.
  if (!method && expr->inPlace) {
    method = findBestMethod(lt.get(), format("__i{}__", magic), {{"", lt}, {"", rt}});
    if (method && noReturn)
      *noReturn = true;
  }
  // Check if lt.__op__(lt, rt) exists.
  if (!method)
    method = findBestMethod(lt.get(), format("__{}__", magic), {{"", lt}, {"", rt}});
  // Check if rt.__rop__(rt, lt) exists.
  if (!method)
    method = findBestMethod(rt.get(), format("__r{}__", magic), {{"", rt}, {"", lt}});
  if (!method)
    method = findBestMethod(lt.get(), format("__{}__", magic), {{"", lt}, {"", rt}});
  if (!method)
    error("cannot find magic '{}' in {}", magic, lt->toString());

  return transform(
      N<CallExpr>(N<IdExpr>(method->name), move(expr->lexpr), move(expr->rexpr)));
}

ExprPtr TypecheckVisitor::transformStaticTupleIndex(ClassType *tuple, ExprPtr &expr,
                                                    ExprPtr &index) {
  if (!tuple->isRecord())
    return nullptr;
  if (!startswith(tuple->name, "Tuple.N")) {
    // TODO: be smarter! there might be a compatible getitem?
    return nullptr;
  }
  // TODO: Ptr/Array... Optional unpacking?

  // Extract a static integer value from a compatible expression.
  auto getInt = [](seq_int_t *o, const ExprPtr &e) {
    if (!e)
      return true;
    if (e->isStaticExpr) {
      seqassert(e->staticEvaluation.first, "{} not evaluated", e->toString());
      *o = e->staticEvaluation.second;
      return true;
    }
    if (auto ei = e->getInt()) {
      *o = ei->intValue;
      return true;
    }
    return false;
  };

  auto classItem = ctx->cache->classes.find(tuple->name);
  seqassert(classItem != ctx->cache->classes.end(), "cannot find class '{}'",
            tuple->name);
  seq_int_t start = 0, stop = tuple->args.size(), step = 1;
  if (getInt(&start, index)) {
    int i = translateIndex(start, stop);
    if (i < 0 || i >= stop)
      error("tuple index out of range (expected 0..{}, got {})", stop, i);
    return transform(N<DotExpr>(move(expr), classItem->second.fields[i].name));
  } else if (auto es = CAST(index, SliceExpr)) {
    if (!getInt(&start, es->start) || !getInt(&stop, es->stop) ||
        !getInt(&step, es->step))
      return nullptr;
    // Correct slice indices.
    if (es->step && !es->start)
      start = step > 0 ? 0 : tuple->args.size();
    if (es->step && !es->stop)
      stop = step > 0 ? tuple->args.size() : 0;
    sliceAdjustIndices(tuple->args.size(), &start, &stop, step);
    // Generate new tuple.
    vector<ExprPtr> te;
    for (auto i = start; (step >= 0) ? (i < stop) : (i >= stop); i += step) {
      if (i < 0 || i >= tuple->args.size())
        error("tuple index out of range (expected 0..{}, got {})", tuple->args.size(),
              i);
      te.push_back(N<DotExpr>(clone(expr), classItem->second.fields[i].name));
    }
    return transform(N<CallExpr>(
        N<DotExpr>(N<IdExpr>(format("Tuple.N{}", te.size())), "__new__"), move(te)));
  }
  return nullptr;
}

ExprPtr TypecheckVisitor::transformDot(DotExpr *expr, vector<CallExpr::Arg> *args) {
  auto isMethod = [&](const FuncType *f) {
    auto ast = ctx->cache->functions[f->name].ast.get();
    return in(ast->attributes, ATTR_NOT_STATIC);
  };

  if (expr->member == "__class__") {
    expr->expr = transform(expr->expr, true, true);
    expr->type |= ctx->findInternal("str");
    if (auto t = realizeType(expr->expr->type))
      return transform(N<StringExpr>(t->realizeString()));
    return nullptr;
  }

  expr->expr = transform(expr->expr, true);
  if (expr->member == "__atomic__") {
    expr->type |= ctx->findInternal("bool");
    if (auto t = realizeType(expr->expr->type)) {
      expr->done = true;
    }
    return nullptr;
  } else if (expr->member == "__elemsize__") {
    expr->type |= ctx->findInternal("int");
    if (auto t = realizeType(expr->expr->type)) {
      expr->done = true;
    }
    return nullptr;
  }

  // Case 1: type not yet known, so just assign an unbound type and wait until the
  // next iteration.
  if (expr->expr->getType()->getUnbound()) {
    expr->type |= ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel);
    return nullptr;
  }

  auto typ = expr->expr->getType()->getClass();
  seqassert(typ, "expected formed type: {}", typ->toString());
  auto methods = ctx->findMethod(typ->name, expr->member);
  if (methods.empty()) {
    if (auto member = ctx->findMember(typ->name, expr->member)) {
      // Case 2: Object member access.
      auto t = ctx->instantiate(getSrcInfo(), member, typ.get());
      LOG_TYPECHECK("[inst] {} -> {}", expr->toString(), t->toString());
      expr->type |= t;
      expr->done = expr->expr->done && realizeType(expr->type) != nullptr;
      return nullptr;
    } else if (typ->name == "Optional") {
      // Case 3: Transform optional.member to unwrap(optional).member.
      auto d = N<DotExpr>(transform(N<CallExpr>(N<IdExpr>("unwrap"), move(expr->expr))),
                          expr->member);
      if (auto dd = transformDot(d.get(), args))
        return dd;
      return d;
    } else if (typ->name == "pyobj") {
      // Case 4: Transform pyobj.member to pyobj._getattr("member").
      return transform(N<CallExpr>(N<DotExpr>(move(expr->expr), "_getattr"),
                                   N<StringExpr>(expr->member)));
    } else {
      error("cannot find '{}' in {}", expr->member, typ->toString());
    }
  }

  // Case 5: look for a method that best matches the given arguments.
  //         If it exists, return a simple IdExpr with that method's name.
  //         Append a "self" variable to the front if needed.
  if (args) {
    vector<pair<string, TypePtr>> argTypes;
    bool isType = expr->expr->isType();
    if (!isType)
      argTypes.emplace_back(make_pair("", typ)); // self variable
    for (const auto &a : *args)
      argTypes.emplace_back(make_pair(a.name, a.value->getType()));
    if (auto bestMethod = findBestMethod(typ.get(), expr->member, argTypes)) {
      ExprPtr e = N<IdExpr>(bestMethod->name);
      auto t = ctx->instantiate(getSrcInfo(), bestMethod, typ.get());
      LOG_TYPECHECK("[inst] {} -> {}", expr->toString(), t->toString());
      e->type |= t;
      expr->type |= e->type;
      if (!isType)
        args->insert(args->begin(), {"", move(expr->expr)}); // self variable
      e = transform(e); // Visit IdExpr and realize it if necessary.
      // Remove lingering unbound variables from expr->expr (typ) instantiation
      // if we accessed a method that does not reference any generic in typ.
      if (isType && !isMethod(bestMethod.get()))
        deactivateUnbounds(typ.get());
      return e;
    }
    // No method was found, print a nice error message.
    vector<string> nice;
    for (auto &t : argTypes)
      nice.emplace_back(format("{} = {}", t.first, t.second->toString()));
    error("cannot find a method '{}' in {} with arguments {}", expr->member,
          typ->toString(), join(nice, ", "));
  }

  // Case 6: multiple overloaded methods available.
  FuncTypePtr bestMethod = nullptr;
  auto oldType = expr->getType() ? expr->getType()->getClass() : nullptr;
  if (methods.size() > 1 && oldType && startswith(oldType->name, "Function.N")) {
    // If old type is already a function, use its argument types to pick the best
    // call.
    vector<pair<string, TypePtr>> methodArgs;
    if (!expr->expr->isType()) // self argument
      methodArgs.emplace_back(make_pair("", typ));
    for (auto i = 1; i < oldType->explicits.size(); i++)
      methodArgs.emplace_back(make_pair("", oldType->explicits[i].type));
    bestMethod = findBestMethod(typ.get(), expr->member, methodArgs);
    if (!bestMethod) {
      // Print a nice error message.
      vector<string> nice;
      for (auto &t : methodArgs)
        nice.emplace_back(format("{} = {}", t.first, t.second->toString()));
      error("cannot find a method '{}' in {} with arguments {}", expr->member,
            typ->toString(), join(nice, ", "));
    }
  } else {
    // HACK: if we still have multiple valid methods, we just use the first one.
    // TODO: handle this better (maybe hold these types until they can be selected?)
    bestMethod = methods[0];
  }

  // Case 7: only one valid method remaining. Check if this is a class method or an
  // object method access and transform accordingly.
  if (expr->expr->isType()) {
    // Class method access: Type.method.
    auto name = bestMethod->name;
    auto val = ctx->find(name);
    seqassert(val, "cannot find method '{}'", name);
    ExprPtr e = N<IdExpr>(name);
    auto t = ctx->instantiate(getSrcInfo(), bestMethod, typ.get());
    LOG_TYPECHECK("[inst] {} -> {}", expr->toString(), t->toString());
    e->type |= t;
    expr->type |= e->type;
    e = transform(e); // Visit IdExpr and realize it if necessary.
    // Remove lingering unbound variables from expr->expr (typ) instantiation
    // if we accessed a method that does not reference any generic in typ.
    if (!isMethod(bestMethod.get()))
      deactivateUnbounds(typ.get());
    return e;
  } else {
    // Object access: y.method. Transform y.method to a partial call
    // typeof(t).foo(y, ...).
    vector<ExprPtr> methodArgs;
    methodArgs.push_back(move(expr->expr));
    for (int i = 0; i < std::max(1, (int)bestMethod->args.size() - 2); i++)
      methodArgs.push_back(N<EllipsisExpr>());
    // Handle @property methods.
    if (in(ctx->cache->functions[bestMethod->name].ast->attributes, "property"))
      methodArgs.pop_back();
    ExprPtr e = N<CallExpr>(N<IdExpr>(bestMethod->name), move(methodArgs));
    return transform(e, false, allowVoidExpr);
  }
}

void TypecheckVisitor::deactivateUnbounds(Type *t) {
  auto ub = t->getUnbounds();
  for (auto &u : ub)
    ctx->activeUnbounds.erase(u);
}

FuncTypePtr
TypecheckVisitor::findBestMethod(ClassType *typ, const string &member,
                                 const vector<pair<string, types::TypePtr>> &args) {
  auto methods = ctx->findMethod(typ->name, member);
  if (methods.empty())
    return nullptr;
  if (methods.size() == 1) // methods is not overloaded
    return methods[0];

  // Calculate the unification score for each available methods and pick the one with
  // highest score.
  vector<pair<int, int>> scores;
  for (int mi = 0; mi < methods.size(); mi++) {
    auto method = dynamic_pointer_cast<FuncType>(
        ctx->instantiate(getSrcInfo(), methods[mi], typ, false));

    auto reordered = reorderNamedArgs(method.get(), args);
    if (reordered.first == -1)
      continue;
    // Scoring system for each argument:
    //   Generics, traits and default arguments get a score of zero (lowest priority).
    //   Optional unwrap gets the score of 1.
    //   Optional wrap gets the score of 2.
    //   Successful unification gets the score of 3 (highest priority).
    int score = reordered.first;
    for (int ai = 0; ai < reordered.second.size(); ai++) {
      auto expectedType = method->args[ai + 1];
      auto expectedClass = expectedType->getClass();
      auto argType = reordered.second[ai].second;
      auto argClass = argType->getClass();
      // Ignore traits and default arguments.
      if ((expectedClass && expectedClass->isTrait) || !reordered.second[ai].second)
        continue;

      Type::Unification undo;
      int u = argType->unify(expectedType.get(), &undo);
      undo.undo();
      if (u >= 0) {
        score += u + 3;
        continue;
      }
      // Unification failed: maybe we need to wrap an argument?
      if (expectedClass && expectedClass->name == "Optional" && argClass &&
          argClass->name != expectedClass->name) {
        u = argType->unify(expectedClass->explicits[0].type.get(), &undo);
        undo.undo();
        if (u >= 0) {
          score += u + 2;
          continue;
        }
      }
      // ... or unwrap it (less ideal)?
      if (argClass && argClass->name == "Optional" && expectedClass &&
          argClass->name != expectedClass->name) {
        u = argClass->explicits[0].type->unify(expectedType.get(), &undo);
        undo.undo();
        if (u >= 0) {
          score += u;
          continue;
        }
      }
      // This method cannot be selected, ignore it.
      score = -1;
      break;
    }
    if (score >= 0)
      scores.emplace_back(std::make_pair(score, mi));
  }
  if (scores.empty())
    return nullptr;
  // Get the best score.
  sort(scores.begin(), scores.end(), std::greater<>());
  return methods[scores[0].second];
}

pair<int, vector<pair<string, TypePtr>>>
TypecheckVisitor::reorderNamedArgs(const FuncType *func,
                                   const vector<pair<string, TypePtr>> &args) {
  vector<int> argIndex;
  for (int i = 0; i < int(func->args.size()) - 1; i++)
    argIndex.push_back(i);
  unordered_map<string, TypePtr> namedArgs;
  vector<pair<string, TypePtr>> reorderedArgs;
  for (const auto &a : args) {
    if (a.first.empty())
      reorderedArgs.emplace_back(make_pair("", a.second));
    else
      namedArgs[a.first] = a.second;
  }
  // TODO: check for *args
  if (reorderedArgs.size() + namedArgs.size() != argIndex.size())
    return {-1, {}};
  // Final score: +2 for each matched argument, +1 for each missing default argument
  int score = int(reorderedArgs.size()) * 2;
  FunctionStmt *ast = ctx->cache->functions[func->name].ast.get();
  seqassert(ast, "AST not accessible for {}", func->name);
  for (int i = reorderedArgs.size(); i < argIndex.size(); i++) {
    auto matchingName =
        ctx->cache->reverseIdentifierLookup[ast->args[argIndex[i]].name];
    auto it = namedArgs.find(matchingName);
    if (it != namedArgs.end()) {
      reorderedArgs.emplace_back(make_pair("", it->second));
      namedArgs.erase(it);
      score += 2;
    } else if (ast->args[i].deflt) {
      if (ast->args[argIndex[i]].type)
        reorderedArgs.emplace_back(make_pair("", func->args[argIndex[i] + 1]));
      else
        reorderedArgs.emplace_back(std::make_pair("", nullptr));
      score += 1;
    } else {
      return {-1, {}};
    }
  }
  if (!namedArgs.empty())
    return {-1, {}};
  return make_pair(score, reorderedArgs);
}

ExprPtr TypecheckVisitor::transformCall(CallExpr *expr, const types::TypePtr &inType,
                                        ExprPtr *extraStage) {
  auto special = transformSpecialCall(expr);
  if (special.first)
    return move(special.second);

  ExprPtr oldExpr = nullptr;
  if (extraStage)
    oldExpr = expr->clone(); // keep the old expression if we end up with an extra stage

  // First transform the arguments.
  for (auto &a : expr->args) {
    a.value = transform(a.value);
    // Unbound inType might become a generator that will need to be extracted, so
    // don't unify it yet.
    if (inType && !inType->getUnbound() && a.value->getEllipsis() &&
        a.value->getEllipsis()->isPipeArg)
      a.value->type |= inType;
  }

  // Intercept dot-callees (e.g. expr.foo). Needed in order to select a proper
  // overload for magic methods and to avoid dealing with partial calls
  // (a non-intercepted object DotExpr (e.g. expr.foo) will get transformed into a
  // partial call).
  ExprPtr *lhs = &expr->expr;
  // Make sure to check for instantiation DotExpr (e.g. a.b[T]) as well.
  if (auto ei = const_cast<IndexExpr *>(
          expr->expr->getIndex())) // A potential function instantiation
    lhs = &ei->expr;
  else if (auto eii = CAST(expr->expr, InstantiateExpr)) // Real instantiation
    lhs = &eii->typeExpr;
  if (auto ed = const_cast<DotExpr *>((*lhs)->getDot())) {
    if (auto edt = transformDot(ed, &expr->args))
      *lhs = move(edt);
  }
  expr->expr = transform(expr->expr, true);

  auto calleeType = expr->expr->getType();
  auto calleeClass = expr->expr->getType()->getClass();
  if (!calleeClass) {
    // Case 1: Unbound callee, will be resolved later.
    expr->type |= ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel);
    return nullptr;
  } else if (expr->expr->isType() && calleeClass->isRecord()) {
    // Case 2: Tuple constructor. Transform to: t.__new__(args)
    return transform(
        N<CallExpr>(N<DotExpr>(move(expr->expr), "__new__"), move(expr->args)));
  } else if (expr->expr->isType()) {
    // Case 3: Type constructor. Transform to a StmtExpr:
    //   c = t.__new__(); c.__init__(args); c
    ExprPtr var = N<IdExpr>(ctx->cache->getTemporaryVar("v"));
    vector<StmtPtr> stmts;
    stmts.push_back(N<AssignStmt>(
        clone(var), N<CallExpr>(N<DotExpr>(move(expr->expr), "__new__"))));
    stmts.push_back(
        N<ExprStmt>(N<CallExpr>(N<DotExpr>(clone(var), "__init__"), move(expr->args))));
    return transform(N<StmtExpr>(move(stmts), clone(var)));
  } else if (!calleeClass->getCallable()) {
    // Case 4: callee is not a function nor a partial type. Route it through a
    // __call__ method.
    if (calleeClass->name == "pyobj" &&
        (expr->args.size() != 1 ||
         !(expr->args[0].value->getType()->getClass() &&
           startswith(expr->args[0].value->getType()->getClass()->name, "Tuple.N")))) {
      // HACK: Wrap pyobj arguments in a tuple.
      // TODO: remove this once *args support lands (might be some issues with passing
      // a tuple itself!).
      vector<ExprPtr> e;
      for (auto &a : expr->args) {
        if (!a.name.empty())
          error("named python calls are not yet supported");
        e.push_back(move(a.value));
      }
      auto ne = transform(N<CallExpr>(
          N<DotExpr>(N<IdExpr>(format("Tuple.N{}", expr->args.size())), "__new__"),
          move(e)));
      expr->args.clear();
      expr->args.push_back({"", move(ne)});
    }
    return transform(
        N<CallExpr>(N<DotExpr>(move(expr->expr), "__call__"), move(expr->args)));
  }

  // Case 5: we are calling a function!
  FunctionStmt *ast = nullptr; // might be nullptr if calling a function pointer.
  if (auto fn = calleeType->getFunc())
    ast = ctx->cache->functions[fn->name].ast.get();
  string knownTypes; // A string that contains 0 if an argument is already "known"
                     // (i.e. if we are handling a partial function with stored
                     // arguments) or 0 if it is available.
  if (startswith(calleeClass->name, "Partial.N")) {
    knownTypes = calleeClass->name.substr(9);
    calleeType = calleeClass->args[0]; // This is Function[...] type.
    calleeClass = calleeClass->args[0]->getClass();
    seqassert(calleeClass, "calleeClass not set");
  }

  // Handle named and default arguments.
  vector<int> argIndex;
  for (int i = 0; i < int(calleeClass->args.size()) - 1; i++)
    if (knownTypes.empty() || knownTypes[i] == '0')
      argIndex.push_back(i);
  vector<CallExpr::Arg> reorderedArgs;
  unordered_map<string, ExprPtr> namedArgs;
  for (auto &a : expr->args) {
    if (a.name.empty())
      reorderedArgs.push_back({"", move(a.value)});
    else
      namedArgs[a.name] = move(a.value);
  }
  vector<int> pending;
  bool isPartial = false;
  if (namedArgs.empty() && reorderedArgs.size() == argIndex.size() + 1 &&
      reorderedArgs.back().value->getEllipsis()) {
    // Special case: foo(a, b, ...) for foo(a, b) (trailing ellipsis makes this a
    // singleton partial call).
    isPartial = true;
    // Unify the ellipsis with void (as it won't be used anymore)
    reorderedArgs.back().value->type |= ctx->findInternal("void");
    reorderedArgs.pop_back();
  } else if (reorderedArgs.size() + namedArgs.size() > argIndex.size()) {
    error("too many arguments for {} (expected {}, got {})", calleeType->toString(),
          argIndex.size(), reorderedArgs.size() + namedArgs.size());
  }

  if (ast) {
    ctx->addBlock();
    addFunctionGenerics(calleeType->getFunc().get()); // needed for default arguments.
  } else if (!ast && !namedArgs.empty()) {
    error("unexpected named argument '{}' (function pointers cannot have named "
          "arguments)",
          namedArgs.begin()->first);
  }

  for (int i = 0, ra = reorderedArgs.size(); i < argIndex.size(); i++) {
    if (i >= ra) {
      auto matchingName =
          ctx->cache->reverseIdentifierLookup[ast->args[argIndex[i]].name];
      auto it = namedArgs.find(matchingName);
      if (it != namedArgs.end()) {
        reorderedArgs.push_back({"", move(it->second)});
        namedArgs.erase(it);
      } else if (ast->args[argIndex[i]].deflt) {
        // Use the default argument!
        reorderedArgs.push_back({"", transform(clone(ast->args[argIndex[i]].deflt))});
      } else {
        error("missing argument '{}'", matchingName);
      }
    }
    if (reorderedArgs[i].value->getEllipsis() &&
        !reorderedArgs[i].value->getEllipsis()->isPipeArg)
      pending.push_back(argIndex[i]);
  }
  for (auto &i : namedArgs)
    error(i.second, "unknown argument '{}'", i.first);
  if (isPartial || !pending.empty())
    pending.push_back(expr->args.size());

  // Typecheck given arguments with the expected (signature) types.
  bool unificationsDone = true;
  for (int ri = 0; ri < reorderedArgs.size(); ri++) {
    auto expectedTyp = calleeClass->args[argIndex[ri] + 1];
    auto expectedClass = expectedTyp->getClass();
    auto argClass = reorderedArgs[ri].value->getType()->getClass();

    if (expectedClass &&
        (expectedClass->isTrait || expectedClass->name == "Optional")) {
      if (!argClass) {
        // Case 0: argument type not yet known.
        unificationsDone = false;
      } else if (expectedClass->name == "Generator" && argClass &&
                 argClass->name != expectedClass->name && !extraStage) {
        // Case 1: wrap expected generators with iter().
        // Note: do not do this in pipelines (TODO: why?).
        reorderedArgs[ri].value = transform(
            N<CallExpr>(N<DotExpr>(move(reorderedArgs[ri].value), "__iter__")));
        reorderedArgs[ri].value->type |= expectedClass;
      } else if (expectedClass->name == "Optional" && argClass &&
                 argClass->name != expectedClass->name) {
        // Case 2: wrap expected optionals with Optional().
        if (extraStage && reorderedArgs[ri].value->getEllipsis()) {
          // Check if this is a pipe call...
          *extraStage = N<DotExpr>(N<IdExpr>("Optional"), "__new__");
          return oldExpr;
        }
        reorderedArgs[ri].value = transform(
            N<CallExpr>(N<IdExpr>("Optional"), move(reorderedArgs[ri].value)));
        reorderedArgs[ri].value->type |= expectedClass;
      } else if (ast && startswith(expectedClass->name, "Function.N") && argClass &&
                 !startswith(argClass->name, "Function.N")) {
        // Case 3: allow any callable to match Function[] signature.
        // TODO: this is only allowed with Seq function calls (not function pointers).
        if (!startswith(argClass->name, "Partial.N")) {
          reorderedArgs[ri].value =
              transform(N<DotExpr>(move(reorderedArgs[ri].value), "__call__"));
          argClass = reorderedArgs[ri].value->getType()->getClass();
        }
        if (argClass && startswith(argClass->name, "Partial.N")) {
          // Handle partial call arguments.
          argClass->explicits[0].type |= expectedClass->explicits[0].type;
          if (argClass->explicits.size() != expectedClass->explicits.size() + 1)
            error("incompatible partial type");
          for (int j = 1; j < expectedClass->explicits.size(); j++)
            argClass->explicits[j + 1].type |= expectedClass->explicits[j].type;
          expr->expr->getType()->getFunc()->args[ri + 1] = argClass;
        } else {
          reorderedArgs[ri].value->type |= expectedTyp;
        }
      } else {
        // Case 4: normal unification.
        reorderedArgs[ri].value->type |= expectedTyp;
      }
    } else if (expectedClass && argClass && argClass->name == "Optional" &&
               argClass->name != expectedClass->name) { // unwrap optional
      // Case 5: Optional unwrapping.
      if (extraStage && reorderedArgs[ri].value->getEllipsis()) {
        *extraStage = N<IdExpr>("unwrap");
        return oldExpr;
      }
      reorderedArgs[ri].value =
          transform(N<CallExpr>(N<IdExpr>("unwrap"), move(reorderedArgs[ri].value)));
      reorderedArgs[ri].value->type |= expectedTyp;
    } else {
      // Case 6: normal unification.
      reorderedArgs[ri].value->type |= expectedTyp;
    }
  }
  if (ast)
    ctx->popBlock();

  // Realize arguments.
  expr->done = true;
  for (auto &ra : reorderedArgs) {
    if (realizeType(ra.value->type))
      ra.value = transform(ra.value);
    expr->done &= ra.value->done;
  }
  if (auto f = calleeType->getFunc()) {
    // Handle default generics (callee.g. foo[S, T=int]) only if all arguments were
    // unified.
    if (unificationsDone)
      for (int i = 0; i < f->explicits.size(); i++)
        if (ast->generics[i].deflt && f->explicits[i].type->getUnbound()) {
          LOG("- def: {}", calleeType->toString());
          for (auto &r : reorderedArgs)
            LOG("  - {}", r.value->type->toString());

          auto deflt = clone(ast->generics[i].deflt);
          if (deflt->getNone())
            f->explicits[i].type |=
                ctx->instantiate(getSrcInfo(), ctx->findInternal("T.None"));
          else
            f->explicits[i].type |= transformType(deflt)->getType();

          LOG("- def: {} => {}", deflt->toString(), calleeType->toString());
        }
    if (realizeFunc(f))
      expr->expr = transform(expr->expr);
    expr->done &= expr->expr->done;
  }

  // Emit the final call.
  if (!pending.empty()) {
    // Case 1: partial call.
    // Transform callee(args...) to Partial.N<known>(callee, args...).
    pending.pop_back();
    string known(calleeClass->args.size() - 1, '1');
    for (auto p : pending)
      known[p] = '0';
    auto partialType = generatePartialStub(known, knownTypes);
    vector<ExprPtr> args;
    args.push_back(move(expr->expr));
    for (auto &r : reorderedArgs)
      if (!r.value->getEllipsis())
        args.push_back(move(r.value));
    return transform(N<CallExpr>(N<IdExpr>(partialType), move(args)));
  } else if (!knownTypes.empty()) {
    // Case 2: fulfilled partial call.
    // Transform p(args...) to p.__call__(args...).
    ExprPtr e =
        N<CallExpr>(N<DotExpr>(move(expr->expr), "__call__"), move(reorderedArgs));
    return transform(e, false, allowVoidExpr);
  } else {
    // Case 3. Normal function call.
    expr->args = move(reorderedArgs);
    expr->type |= calleeClass->args[0]; // function return type
    return nullptr;
  }
}

pair<bool, ExprPtr> TypecheckVisitor::transformSpecialCall(CallExpr *expr) {
  if (!expr->expr->getId())
    return {false, nullptr};
  auto val = expr->expr->getId()->value;
  if (val == "isinstance") {
    // Make sure not to activate new unbound here, as we just need to check type
    // equality.
    auto oldActivation = ctx->allowActivation;
    ctx->allowActivation = false;
    expr->args[0].value = transform(expr->args[0].value, true);
    ctx->allowActivation = oldActivation;
    if (auto t = realizeType(expr->args[0].value->getType()))
      expr->args[0].value->type |= t;
    auto typ = expr->args[0].value->type;
    if (!typ) {
      return {true, nullptr};
    } else {
      if (expr->args[1].value->isId("Tuple") || expr->args[1].value->isId("tuple")) {
        return {true, transform(N<BoolExpr>(typ->getClass()->isRecord()))};
      } else if (expr->args[1].value->getNone() && expr->args[0].value->isType()) {
        return {true, transform(N<BoolExpr>(typ->is("T.None")))};
      } else {
        ctx->allowActivation = false;
        expr->args[1].value = transformType(expr->args[1].value);
        ctx->allowActivation = oldActivation;
        auto t = expr->args[1].value->type;
        return {true, transform(N<BoolExpr>(typ->unify(t.get(), nullptr) >= 0))};
      }
    }
  } else if (val == "staticlen") {
    expr->args[0].value = transform(expr->args[0].value);
    auto typ = expr->args[0].value->getType()->getClass();
    if (!typ->getClass())
      return {true, nullptr};
    else if (!typ->isRecord())
      error("{} is not a tuple type", typ->toString());
    else
      return {true, transform(N<IntExpr>(typ->args.size()))};
  } else if (val == "hasattr") {
    auto member = expr->args[1].value->getString()->value;
    auto oldActivation = ctx->allowActivation;
    ctx->allowActivation = false;
    expr->args[0].value = transformType(expr->args[0].value);
    auto typ = expr->args[0].value->getType()->getClass();
    ctx->allowActivation = oldActivation;
    if (!typ)
      return {true, nullptr};
    else
      return {true, transform(N<BoolExpr>(
                        !ctx->findMethod(typ->getClass()->name, member).empty() ||
                        ctx->findMember(typ->getClass()->name, member)))};
  } else if (val == "compile_error") {
    error("custom error: {}", expr->args[0].value->getString()->value);
  }
  return {false, nullptr};
}

void TypecheckVisitor::addFunctionGenerics(const FuncType *t) {
  for (auto p = t->parent; p;) {
    if (auto f = p->getFunc()) {
      for (auto &g : f->explicits)
        if (auto s = g.type->getStatic())
          ctx->add(TypecheckItem::Type, g.name, s, true);
        else if (!g.name.empty())
          ctx->add(TypecheckItem::Type, g.name, g.type);
      p = f->parent;
    } else {
      auto c = p->getClass();
      seqassert(c, "not a class: {}", p->toString());
      for (auto &g : c->explicits)
        if (auto s = g.type->getStatic())
          ctx->add(TypecheckItem::Type, g.name, s, true);
        else if (!g.name.empty())
          ctx->add(TypecheckItem::Type, g.name, g.type);
      p = c->parent;
    }
  }
  for (auto &g : t->explicits)
    if (auto s = g.type->getStatic())
      ctx->add(TypecheckItem::Type, g.name, s, true);
    else if (!g.name.empty())
      ctx->add(TypecheckItem::Type, g.name, g.type);
}

string TypecheckVisitor::generatePartialStub(const string &mask,
                                             const string &oldMask) {
  auto typeName = format("Partial.N{}", mask);
  if (!ctx->find(typeName)) {
    vector<Param> generics, args, missingArgs;
    vector<ExprPtr> genericNames, callArgs;
    args.emplace_back(Param{"ptr", nullptr, nullptr});
    missingArgs.emplace_back(Param{"self", nullptr, nullptr});
    for (int i = 0; i <= mask.size(); i++) {
      genericNames.emplace_back(N<IdExpr>(format("T{}", i)));
      generics.emplace_back(Param{format("T{}", i), nullptr, nullptr});
      if (i && mask[i - 1] == '1') {
        args.emplace_back(
            Param{format("a{0}", i), N<IdExpr>(format("T{}", i)), nullptr});
        callArgs.emplace_back(N<DotExpr>(N<IdExpr>("self"), format("a{0}", i)));
      } else if (i && mask[i - 1] == '0') {
        missingArgs.emplace_back(
            Param{format("a{0}", i), N<IdExpr>(format("T{}", i)), nullptr});
        callArgs.emplace_back(N<IdExpr>(format("a{0}", i)));
      }
    }
    args[0].type = N<IndexExpr>(N<IdExpr>(format("Function", mask.size())),
                                N<TupleExpr>(move(genericNames)));
    auto call = N<CallExpr>(N<DotExpr>(N<IdExpr>("self"), "ptr"), move(callArgs));
    StmtPtr func = N<FunctionStmt>(
        "__call__", N<IdExpr>("T0"), vector<Param>{}, move(missingArgs),
        N<IfStmt>(
            N<CallExpr>(N<IdExpr>("isinstance"), N<IdExpr>("T0"), N<IdExpr>("void")),
            N<ExprStmt>(clone(call)), nullptr, N<ReturnStmt>(clone(call))),
        vector<string>{});
    StmtPtr stmt = make_unique<ClassStmt>(
        typeName, move(generics), move(args), move(func),
        vector<string>{ATTR_TUPLE, ATTR_NO(ATTR_TOTAL_ORDERING), ATTR_NO(ATTR_PICKLE),
                       ATTR_NO(ATTR_CONTAINER), ATTR_NO(ATTR_PYTHON)});

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
    args.emplace_back(Param{"p", nullptr, nullptr});
    newArgs.push_back(N<DotExpr>(N<IdExpr>("p"), "ptr"));
    for (int i = 0; i < mask.size(); i++) {
      if (mask[i] == '1' && oldMask[i] == '0') {
        args.emplace_back(Param{format("a{}", i), nullptr, nullptr});
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
