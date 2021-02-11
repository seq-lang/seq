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
  if (auto rt = realize(typ))
    typ |= rt;
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
  if (startswith(expr->value, "Tuple.N"))
    generateTupleStub(std::stoi(expr->value.substr(7)));
  else if (startswith(expr->value, "Function.N"))
    generateFunctionStub(std::stoi(expr->value.substr(10)));
  auto val = ctx->find(expr->value);
  seqassert(val, "cannot find IdExpr '{}'", expr->value);
  if (val->isStatic()) {
    // Evaluate the static expression.
    seqassert(val->type->getStatic(), "{} does not have static type", expr->value);
    val->type->getStatic()->staticEvaluation = {
        true, val->type->getStatic()->staticExpr.second(val->type->getStatic().get())};
    expr->staticEvaluation = {true, val->type->getStatic()->staticEvaluation.second};
    resultExpr = transform(N<IntExpr>(expr->staticEvaluation.second));
    return;
  }
  if (val->isType())
    expr->markType();
  auto t = ctx->instantiate(getSrcInfo(), val->type);
  LOG_TYPECHECK("[inst] {} -> {}", expr->toString(), t->toString());
  expr->type |= t;

  // Check if we can realize the type.
  if (auto rt = realize(expr->type)) {
    expr->type |= rt;
    if (val->kind == TypecheckItem::Type || val->kind == TypecheckItem::Func)
      expr->value = expr->type->realizedName();
    expr->done = true;
  } else {
    expr->done = false;
  }
}

void TypecheckVisitor::visit(TupleExpr *expr) {
  auto name = generateTupleStub(expr->items.size());
  /// TODO: tuple star-expression
  resultExpr = transform(
      N<CallExpr>(N<DotExpr>(N<IdExpr>(name), "__new__"), clone(expr->items)));
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
      resultExpr = transform(N<BoolExpr>(bool(value)));
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
  bool hasGenerator = false;

  // Returns T if t is of type Generator[T].
  auto getIterableType = [&](TypePtr t) {
    if (t->is("Generator")) {
      hasGenerator = true;
      return t->getClass()->generics[0].type;
    }
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
  expr->type |= (hasGenerator ? ctx->findInternal("void") : inType);
}

void TypecheckVisitor::visit(InstantiateExpr *expr) {
  expr->typeExpr = transform(expr->typeExpr, true);
  auto typ = ctx->instantiate(expr->typeExpr->getSrcInfo(), expr->typeExpr->getType());
  LOG_TYPECHECK("[inst] {} -> {}", expr->typeExpr->toString(), typ->toString());
  seqassert(typ->getFunc() || typ->getClass(), "unknown type");
  auto &generics =
      typ->getFunc() ? typ->getFunc()->funcGenerics : typ->getClass()->generics;
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
              staticGenerics.emplace_back(
                  Generic{ei->value, genTyp,
                          genTyp->getLink() ? genTyp->getLink()->id
                          : genTyp->getStatic()->generics.empty()
                              ? 0
                              : genTyp->getStatic()->generics[0].id});
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
        if (expr->typeParams[i]->isStaticExpr &&
            expr->typeParams[i]->staticEvaluation.first)
          generics[i].type |=
              make_shared<StaticType>(expr->typeParams[i]->staticEvaluation.second);
        else if (expr->typeParams[i]->getId())
          generics[i].type |= staticGenerics[0].type;
        else
          generics[i].type |= make_shared<StaticType>(
              staticGenerics,
              std::make_pair(expr->typeParams[i]->clone(),
                             [nctx](const StaticType *t) -> int {
                               if (t->staticEvaluation.first)
                                 return t->staticEvaluation.second;
                               nctx->addBlock();
                               for (auto &g : t->generics)
                                 nctx->add(TypecheckItem::Type, g.name, g.type, true);
                               auto en = TypecheckVisitor(nctx).transform(
                                   t->staticExpr.first->clone());
                               seqassert(en->isStaticExpr && en->staticEvaluation.first,
                                         "{} cannot be evaluated", en->toString());
                               nctx->popBlock();
                               return en->staticEvaluation.second;
                             }),
              std::make_pair(false, 0));
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
  if (auto rt = realize(expr->type)) {
    expr->type |= rt;
    resultExpr = N<IdExpr>(expr->type->realizedName());
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
    if (!resultExpr) {
      // Case 3: ... and if not, just call __getitem__.
      if (auto et = const_cast<TupleExpr *>(expr->index->getTuple()))
        expr->index = N<CallExpr>(
            N<DotExpr>(N<IdExpr>(format("Tuple.N{}", et->items.size())), "__new__"),
            move(et->items));
      ExprPtr e =
          N<CallExpr>(N<DotExpr>(move(expr->expr), "__getitem__"), move(expr->index));
      resultExpr = transform(e, false, allowVoidExpr);
    }
  } else {
    // Case 4: type is still unknown.
    // expr->index = transform(expr->index);
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
  if (auto rt = realize(expr->type)) {
    expr->type |= rt;
    expr->done = expr->expr->done;
  }
}

void TypecheckVisitor::visit(EllipsisExpr *expr) {
  expr->type |= ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel);
}

void TypecheckVisitor::visit(TypeOfExpr *expr) {
  expr->expr = transform(expr->expr);
  expr->type |= expr->expr->type;

  if (auto rt = realize(expr->type)) {
    expr->type |= rt;
    resultExpr = N<IdExpr>(expr->type->realizedName());
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
  expr->type |= typ->getClass()->generics[0].type;
  expr->done = realize(expr->type) != nullptr;
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
    auto lc = realize(expr->lexpr->getType());
    auto rc = realize(expr->rexpr->getType());
    if (!lc || !rc) {
      // We still do not know the exact types...
      expr->type |= ctx->findInternal("bool");
      return nullptr;
    } else if (!lc->getRecord() && !rc->getRecord()) {
      return transform(
          N<BinaryExpr>(N<CallExpr>(N<DotExpr>(move(expr->lexpr), "__raw__")),
                        "==", N<CallExpr>(N<DotExpr>(move(expr->rexpr), "__raw__"))));
    } else if (lc->getClass()->name == "Optional") {
      return transform(N<CallExpr>(N<DotExpr>(move(expr->lexpr), "__is_optional__"),
                                   move(expr->rexpr)));
    } else if (rc->getClass()->name == "Optional") {
      return transform(N<CallExpr>(N<DotExpr>(move(expr->rexpr), "__is_optional__"),
                                   move(expr->lexpr)));
    } else if (lc->realizedName() != rc->realizedName()) {
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
    method = ctx->findBestMethod(lt.get(), format("__atomic_{}__", magic),
                                 {{"", ptrlt}, {"", rt}});
    if (method) {
      expr->lexpr = N<PtrExpr>(move(expr->lexpr));
      if (noReturn)
        *noReturn = true;
    }
  }
  // Check if lt.__iop__(lt, rt) exists.
  if (!method && expr->inPlace) {
    method =
        ctx->findBestMethod(lt.get(), format("__i{}__", magic), {{"", lt}, {"", rt}});
    if (method && noReturn)
      *noReturn = true;
  }
  // Check if lt.__op__(lt, rt) exists.
  if (!method)
    method =
        ctx->findBestMethod(lt.get(), format("__{}__", magic), {{"", lt}, {"", rt}});
  // Check if rt.__rop__(rt, lt) exists.
  if (!method)
    method =
        ctx->findBestMethod(rt.get(), format("__r{}__", magic), {{"", rt}, {"", lt}});
  if (!method)
    method =
        ctx->findBestMethod(lt.get(), format("__{}__", magic), {{"", lt}, {"", rt}});
  if (!method)
    error("cannot find magic '{}' in {}", magic, lt->toString());

  return transform(
      N<CallExpr>(N<IdExpr>(method->funcName), move(expr->lexpr), move(expr->rexpr)));
}

ExprPtr TypecheckVisitor::transformStaticTupleIndex(ClassType *tuple, ExprPtr &expr,
                                                    ExprPtr &index) {
  if (!tuple->getRecord() ||
      in(set<string>{"Ptr", "pyobj", "str", "Array"}, tuple->name))
    // Ptr, pyobj and str are internal types and have only one overloaded __getitem__
    return nullptr;
  if (ctx->cache->classes[tuple->name].methods["__getitem__"].size() != 1)
    // TODO: be smarter! there might be a compatible getitem?
    return nullptr;

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
  auto sz = classItem->second.fields.size();
  seq_int_t start = 0, stop = sz, step = 1;
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
      start = step > 0 ? 0 : sz;
    if (es->step && !es->stop)
      stop = step > 0 ? sz : 0;
    sliceAdjustIndices(sz, &start, &stop, step);
    // Generate new tuple.
    vector<ExprPtr> te;
    for (auto i = start; (step >= 0) ? (i < stop) : (i >= stop); i += step) {
      if (i < 0 || i >= sz)
        error("tuple index out of range (expected 0..{}, got {})", sz, i);
      te.push_back(N<DotExpr>(clone(expr), classItem->second.fields[i].name));
    }
    return transform(N<CallExpr>(
        N<DotExpr>(N<IdExpr>(format("Tuple.N{}", te.size())), "__new__"), move(te)));
  }
  return nullptr;
}

ExprPtr TypecheckVisitor::transformDot(DotExpr *expr, vector<CallExpr::Arg> *args) {
  auto isMethod = [&](const FuncType *f) {
    auto ast = ctx->cache->functions[f->funcName].ast.get();
    return in(ast->attributes, ATTR_NOT_STATIC);
  };

  if (expr->member == "__class__") {
    expr->expr = transform(expr->expr, true, true);
    expr->type |= ctx->findInternal("str");
    if (auto t = realize(expr->expr->type)) {
      if (auto f = t->getFunc())
        return transform(N<StringExpr>(f->RecordType::realizedName()));
      else
        return transform(N<StringExpr>(t->realizedName()));
    }
    return nullptr;
  }
  expr->expr = transform(expr->expr, true);

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
      expr->done = expr->expr->done && realize(expr->type) != nullptr;
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
      // For debugging purposes: ctx->findMethod(typ->name, expr->member);
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
    if (auto bestMethod = ctx->findBestMethod(typ.get(), expr->member, argTypes)) {
      ExprPtr e = N<IdExpr>(bestMethod->funcName);
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
    // If old type is already a function, use its arguments to pick the best call.
    vector<pair<string, TypePtr>> methodArgs;
    if (!expr->expr->isType()) // self argument
      methodArgs.emplace_back(make_pair("", typ));
    for (auto i = 1; i < oldType->generics.size(); i++)
      methodArgs.emplace_back(make_pair("", oldType->generics[i].type));
    bestMethod = ctx->findBestMethod(typ.get(), expr->member, methodArgs);
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
    auto name = bestMethod->funcName;
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
    if (in(ctx->cache->functions[bestMethod->funcName].ast->attributes, "property"))
      methodArgs.pop_back();
    ExprPtr e = N<CallExpr>(N<IdExpr>(bestMethod->funcName), move(methodArgs));
    return transform(e, false, allowVoidExpr);
  }
}

void TypecheckVisitor::deactivateUnbounds(Type *t) {
  auto ub = t->getUnbounds();
  for (auto &u : ub)
    ctx->activeUnbounds.erase(u);
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
  for (int ai = 0; ai < expr->args.size(); ai++) {
    if (auto es = const_cast<StarExpr *>(expr->args[ai].value->getStar())) {
      // Case 1: *arg unpacking
      es->what = transform(es->what);
      auto t = es->what->type->getClass();
      if (!t)
        return nullptr;
      if (!t->getRecord())
        error("can only unpack tuple types");
      auto &ff = ctx->cache->classes[t->name].fields;
      for (int i = 0; i < t->getRecord()->args.size(); i++, ai++)
        expr->args.insert(
            expr->args.begin() + ai,
            CallExpr::Arg{"", transform(N<DotExpr>(clone(es->what), ff[i].name))});
      expr->args.erase(expr->args.begin() + ai);
      ai--;
    } else if (auto ek = CAST(expr->args[ai].value, KeywordStarExpr)) {
      // Case 2: **kwarg unpacking
      ek->what = transform(ek->what);
      auto t = ek->what->type->getClass();
      if (!t)
        return nullptr;
      if (!t->getRecord() || startswith(t->name, "Tuple.N"))
        error("can only unpack named tuple types: {}", t->toString());
      auto &ff = ctx->cache->classes[t->name].fields;
      for (int i = 0; i < t->getRecord()->args.size(); i++, ai++)
        expr->args.insert(expr->args.begin() + ai,
                          CallExpr::Arg{ff[i].name, transform(N<DotExpr>(
                                                        clone(ek->what), ff[i].name))});
      expr->args.erase(expr->args.begin() + ai);
      ai--;
    } else {
      // Case 3: Normal argument
      expr->args[ai].value = transform(expr->args[ai].value);
      // Unbound inType might become a generator that will need to be extracted, so
      // don't unify it yet.
      if (inType && !inType->getUnbound() && expr->args[ai].value->getEllipsis() &&
          expr->args[ai].value->getEllipsis()->isPipeArg)
        expr->args[ai].value->type |= inType;
    }
  }
  set<string> seenNames;
  for (auto &i : expr->args)
    if (!i.name.empty()) {
      if (in(seenNames, i.name))
        error("repeated named argument '{}'", i.name);
      seenNames.insert(i.name);
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

  auto callee = expr->expr->getType()->getClass();
  if (!callee) {
    // Case 1: Unbound callee, will be resolved later.
    expr->type |= ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel);
    return nullptr;
  } else if (expr->expr->isType() && callee->getRecord()) {
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
  } else if (!startswith(callee->name, "Function.N") &&
             !startswith(callee->name, "Partial.N")) {
    // Case 4: callee is not a function nor a partial type. Route it through a
    // __call__ method.
    return transform(
        N<CallExpr>(N<DotExpr>(move(expr->expr), "__call__"), move(expr->args)));
  }

  auto calleeFn = expr->expr->getType()->getRecord();
  // Case 5: we are calling a function!
  string knownTypes; // A string that contains 0 if an argument is already "known"
                     // (i.e. if we are handling a partial function with stored
                     // arguments) or 0 if it is available.
  if (startswith(calleeFn->name, "Partial.N")) {
    knownTypes = calleeFn->name.substr(9);
    calleeFn = calleeFn->args[0]->getRecord(); // This is Function[...] type.
    seqassert(calleeFn, "calleeFn not set");
  }
  FunctionStmt *ast = nullptr; // might be nullptr if calling a function pointer.
  if (auto fn = calleeFn->getFunc())
    ast = ctx->cache->functions[fn->funcName].ast.get();

  // Handle named and default arguments
  vector<CallExpr::Arg> args;
  map<int, int> argIndex;
  ctx->reorderNamedArgs(
      calleeFn.get(), knownTypes, expr->args,
      [&](const map<int, int> &aIdx, int starArgIndex, int kwstarArgIndex,
          const vector<vector<int>> &slots) {
        argIndex = aIdx;
        if (ast) { // add generics for default arguments.
          ctx->addBlock();
          addFunctionGenerics(calleeFn->getFunc().get());
        }
        for (auto &ai : argIndex) {
          if (ai.first == starArgIndex) {
            vector<ExprPtr> extra;
            for (auto &e : slots[ai.second]) {
              if (expr->args[e].value->getEllipsis())
                error(expr->args[e].value, "cannot pass an ellipsis to *args");
              extra.push_back(move(expr->args[e].value));
            }
            args.push_back({"", transform(N<TupleExpr>(move(extra)))});
          } else if (ai.first == kwstarArgIndex) {
            vector<string> names;
            vector<CallExpr::Arg> values;
            for (auto &e : slots[ai.second]) {
              if (expr->args[e].value->getEllipsis())
                error(expr->args[e].value, "cannot pass an ellipsis to **kwargs");
              names.emplace_back(expr->args[e].name);
              values.emplace_back(CallExpr::Arg{"", move(expr->args[e].value)});
            }
            auto name = generateTupleStub(names.size(), "KwTuple", names);
            args.push_back({"", transform(N<CallExpr>(N<IdExpr>(name), move(values)))});
          } else if (slots[ai.first].empty()) {
            auto es = ast->args[ai.first].deflt->toString();
            if (in(ctx->defaultCallDepth, es))
              error("recursive default arguments");
            ctx->defaultCallDepth.insert(es);
            args.push_back({"", transform(clone(ast->args[ai.first].deflt))});
            ctx->defaultCallDepth.erase(es);
          } else {
            seqassert(slots[ai.second].size() == 1, "call transformation failed");
            args.push_back({"", move(expr->args[slots[ai.second][0]].value)});
          }
        }
        if (ast)
          ctx->popBlock();
        return 0;
      },
      [&](const string &errorMsg) {
        error("{}", errorMsg);
        return -1;
      });

  // Form a new mask if this is a partial call
  string newMask;
  for (auto &ai : argIndex)
    if (args[ai.second].value->getEllipsis() &&
        !args[ai.second].value->getEllipsis()->isPipeArg) {
      if (newMask.empty())
        newMask = string(calleeFn->args.size() - 1, '1');
      newMask[ai.first] = '0';
    }

  // Typecheck given arguments with the expected (signature) types.
  bool unificationsDone = true;
  for (auto &ai : argIndex) {
    auto expectedTyp = calleeFn->args[ai.first + 1];
    auto expectedClass = expectedTyp->getClass();
    auto argClass = args[ai.second].value->getType()->getClass();

    if (expectedClass &&
        (expectedClass->isTrait || expectedClass->name == "Optional")) {
      if (!argClass) {
        // Case 0: argument type not yet known.
        unificationsDone = false;
      } else if (expectedClass->name == "Generator" && argClass &&
                 argClass->name != expectedClass->name && !extraStage) {
        // Case 1: wrap expected generators with iter().
        // Note: do not do this in pipelines (TODO: why?).
        args[ai.second].value =
            transform(N<CallExpr>(N<DotExpr>(move(args[ai.second].value), "__iter__")));
        args[ai.second].value->type |= expectedClass;
      } else if (expectedClass->name == "Optional" && argClass &&
                 argClass->name != expectedClass->name) {
        // Case 2: wrap expected optionals with Optional().
        if (extraStage && args[ai.second].value->getEllipsis()) {
          // Check if this is a pipe call...
          *extraStage = N<DotExpr>(N<IdExpr>("Optional"), "__new__");
          return oldExpr;
        }
        args[ai.second].value =
            transform(N<CallExpr>(N<IdExpr>("Optional"), move(args[ai.second].value)));
        args[ai.second].value->type |= expectedClass;
      } else if (ast && startswith(expectedClass->name, "Function.N") && argClass &&
                 (!startswith(argClass->name, "Function.N"))) {
        // Case 3: allow any callable to match Function[] signature.
        if (!startswith(argClass->name, "Partial.N"))
          // Transform it to a Partial.N... via __call__ magic.
          args[ai.second].value =
              transform(N<DotExpr>(move(args[ai.second].value), "__call__"));
        args[ai.second].value->type |= expectedTyp;
        calleeFn->args[ai.first + 1] = args[ai.second].value->type;
      } else {
        // Case 4: normal unification.
        args[ai.second].value->type |= expectedTyp;
      }
    } else if (expectedClass && argClass && argClass->name == "Optional" &&
               argClass->name != expectedClass->name) { // unwrap optional
      // Case 5: Optional unwrapping.
      if (extraStage && args[ai.second].value->getEllipsis()) {
        *extraStage = N<IdExpr>("unwrap");
        return oldExpr;
      }
      args[ai.second].value =
          transform(N<CallExpr>(N<IdExpr>("unwrap"), move(args[ai.second].value)));
      args[ai.second].value->type |= expectedTyp;
    } else {
      // Case 6: normal unification.
      args[ai.second].value->type |= expectedTyp;
    }
  }

  // Realize arguments.
  expr->done = true;
  for (auto &ra : args) {
    if (auto rt = realize(ra.value->type)) {
      rt |= ra.value->type;
      ra.value = transform(ra.value);
    }
    expr->done &= ra.value->done;
  }
  if (auto f = calleeFn->getFunc()) {
    // Handle default generics (calleeFn.g. foo[S, T=int]) only if all arguments were
    // unified.
    if (unificationsDone)
      for (int i = 0; i < f->funcGenerics.size(); i++)
        if (ast->generics[i].deflt && f->funcGenerics[i].type->getUnbound()) {
          auto deflt = clone(ast->generics[i].deflt);
          if (deflt->getNone())
            f->funcGenerics[i].type |=
                ctx->instantiate(getSrcInfo(), ctx->findInternal("T.None"));
          else
            f->funcGenerics[i].type |= transformType(deflt)->getType();
        }
    if (auto rt = realize(f)) {
      rt |= static_pointer_cast<Type>(calleeFn);
      expr->expr = transform(expr->expr);
    }
    expr->done &= expr->expr->done;
  }
  // Emit the final call.
  if (!newMask.empty()) {
    // Case 1: partial call.
    // Transform calleeFn(args...) to Partial.N<known>(calleeFn, args...).
    // LOG("> new partial: {} with {}", calleeFn->toString(), known);
    auto partialType = generatePartialStub(newMask, knownTypes);
    vector<ExprPtr> newArgs;
    newArgs.push_back(move(expr->expr));
    for (auto &r : args)
      if (!r.value->getEllipsis())
        newArgs.push_back(move(r.value));
    return transform(N<CallExpr>(N<IdExpr>(partialType), move(newArgs)));
  } else if (!knownTypes.empty()) {
    // Case 2: fulfilled partial call.
    // Transform p(args...) to p.__call__(args...).
    ExprPtr e = N<CallExpr>(N<DotExpr>(move(expr->expr), "__call__"), move(args));
    return transform(e, false, allowVoidExpr);
  } else {
    // Case 3. Normal function call.
    expr->args = move(args);
    expr->type |= calleeFn->args[0]; // function return type
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
    // if (auto t = realizeType(expr->args[0].value->getType()))
    // expr->args[0].value->type |= t;
    auto typ = expr->args[0].value->type;
    if (!typ) {
      return {true, nullptr};
    } else {
      if (expr->args[1].value->isId("Tuple") || expr->args[1].value->isId("tuple")) {
        return {true, transform(N<BoolExpr>(typ->getRecord() != nullptr))};
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
    else if (!typ->getRecord())
      error("{} is not a tuple type", typ->toString());
    else
      return {true, transform(N<IntExpr>(typ->getRecord()->args.size()))};
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
  for (auto p = t->funcParent; p;) {
    if (auto f = p->getFunc()) {
      for (auto &g : f->funcGenerics)
        if (auto s = g.type->getStatic())
          ctx->add(TypecheckItem::Type, g.name, s, true);
        else if (!g.name.empty())
          ctx->add(TypecheckItem::Type, g.name, g.type);
      p = f->funcParent;
    } else {
      auto c = p->getClass();
      seqassert(c, "not a class: {}", p->toString());
      for (auto &g : c->generics)
        if (auto s = g.type->getStatic())
          ctx->add(TypecheckItem::Type, g.name, s, true);
        else if (!g.name.empty())
          ctx->add(TypecheckItem::Type, g.name, g.type);
      break;
    }
  }
  for (auto &g : t->funcGenerics)
    if (auto s = g.type->getStatic())
      ctx->add(TypecheckItem::Type, g.name, s, true);
    else if (!g.name.empty())
      ctx->add(TypecheckItem::Type, g.name, g.type);
}

string TypecheckVisitor::generateTupleStub(int len, string name, vector<string> names) {
  static map<string, int> usedNames;
  auto key = join(names, ";");
  string suffix;
  if (!names.empty()) {
    if (!in(usedNames, key))
      usedNames[key] = usedNames.size();
    suffix = format("_{}", usedNames[key]);
  } else {
    for (int i = 1; i <= len; i++)
      names.push_back(format("item{}", i));
  }
  auto typeName = format("{}.N{}{}", name, len, suffix);
  if (!ctx->find(typeName)) {
    vector<Param> generics, args;
    for (int i = 1; i <= len; i++) {
      generics.emplace_back(Param{format("T{}", i), nullptr, nullptr});
      args.emplace_back(Param{names[i - 1], N<IdExpr>(format("T{}", i)), nullptr});
    }
    StmtPtr stmt = make_unique<ClassStmt>(typeName, move(generics), move(args), nullptr,
                                          vector<string>{ATTR_TUPLE});
    stmt->setSrcInfo(ctx->cache->generateSrcInfo());

    stmt = SimplifyVisitor::apply(ctx->cache->imports[STDLIB_IMPORT].ctx, stmt,
                                  FILE_GENERATED, 0);
    stmt = TypecheckVisitor(ctx).transform(stmt);
    prependStmts->push_back(move(stmt));
  }
  return typeName;
}

string TypecheckVisitor::generateFunctionStub(int n) {
  seqassert(n >= 0, "invalid n");
  auto typeName = format("Function.N{}", n);
  if (!ctx->find(typeName)) {
    vector<Param> generics;
    generics.emplace_back(Param{"TR", nullptr, nullptr});
    vector<ExprPtr> genericNames;
    genericNames.emplace_back(N<IdExpr>("TR"));
    // TODO: remove this args hack
    vector<Param> args;
    args.emplace_back(Param{"ret", N<IdExpr>("TR"), nullptr});
    for (int i = 1; i <= n; i++) {
      genericNames.emplace_back(N<IdExpr>(format("T{}", i)));
      generics.emplace_back(Param{format("T{}", i), nullptr, nullptr});
      args.emplace_back(Param{format("a{}", i), N<IdExpr>(format("T{}", i)), nullptr});
    }
    ExprPtr type = N<IndexExpr>(N<IdExpr>(typeName), N<TupleExpr>(move(genericNames)));

    vector<StmtPtr> fns;
    vector<Param> params;
    vector<StmtPtr> stmts;
    // def __new__(what: Ptr[byte]) -> Function.N[TR, T1, ..., TN]:
    //   return __internal__.fn_new[Function.N[TR, ...]](what)
    params.emplace_back(
        Param{"what", N<IndexExpr>(N<IdExpr>("Ptr"), N<IdExpr>("byte"))});
    stmts.push_back(N<ReturnStmt>(N<CallExpr>(
        N<IndexExpr>(N<DotExpr>(N<IdExpr>("__internal__"), "fn_new"), clone(type)),
        N<IdExpr>("what"))));
    fns.emplace_back(make_unique<FunctionStmt>("__new__", clone(type), vector<Param>{},
                                               move(params), N<SuiteStmt>(move(stmts)),
                                               vector<string>{}));
    params.clear();
    stmts.clear();
    // def __raw__(self: Function.N[TR, T1, ..., TN]) -> Ptr[byte]:
    //   return __internal__.fn_raw(self)
    params.emplace_back(Param{"self", clone(type)});
    stmts.push_back(N<ReturnStmt>(N<CallExpr>(
        N<DotExpr>(N<IdExpr>("__internal__"), "fn_raw"), N<IdExpr>("self"))));
    fns.emplace_back(make_unique<FunctionStmt>(
        "__raw__", N<IndexExpr>(N<IdExpr>("Ptr"), N<IdExpr>("byte")), vector<Param>{},
        move(params), N<SuiteStmt>(move(stmts)), vector<string>{}));
    params.clear();
    stmts.clear();
    // def __str__(self: Function.N[TR, T1, ..., TN]) -> str:
    //   return __internal__.raw_type_str(self.__raw__(), "function")
    params.emplace_back(Param{"self", clone(type)});
    stmts.push_back(
        N<ReturnStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>("__internal__"), "raw_type_str"),
                                  N<CallExpr>(N<DotExpr>(N<IdExpr>("self"), "__raw__")),
                                  N<StringExpr>("function"))));
    fns.emplace_back(make_unique<FunctionStmt>(
        "__str__", N<IdExpr>("str"), vector<Param>{}, move(params),
        N<SuiteStmt>(move(stmts)), vector<string>{}));
    // class Function.N[TR, T1, ..., TN]
    StmtPtr stmt = make_unique<ClassStmt>(
        typeName, move(generics), move(args), N<SuiteStmt>(move(fns)),
        vector<string>{ATTR_INTERNAL, ATTR_TRAIT, ATTR_TUPLE});
    stmt->setSrcInfo(ctx->cache->generateSrcInfo());

    // Parse this function in a clean context.
    stmt = SimplifyVisitor::apply(ctx->cache->imports[STDLIB_IMPORT].ctx, stmt,
                                  FILE_GENERATED, 0);
    stmt = TypecheckVisitor(ctx).transform(stmt);
    prependStmts->push_back(move(stmt));
  }
  return typeName;
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
                                  FILE_GENERATED, 0);
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
                                  FILE_GENERATED, 0);
    stmt = TypecheckVisitor(ctx).transform(stmt);
    prependStmts->push_back(move(stmt));
  }
  return fnName;
}

} // namespace ast
} // namespace seq
