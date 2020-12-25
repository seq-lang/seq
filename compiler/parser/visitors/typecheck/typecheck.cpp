/**
 * TODO : Redo error messages (right now they are awful)
 */

#include "util/fmt/format.h"
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "parser/ast.h"
#include "parser/common.h"
#include "parser/visitors/simplify/simplify_ctx.h"
#include "parser/visitors/typecheck/typecheck.h"
#include "parser/visitors/typecheck/typecheck_ctx.h"

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

TypecheckVisitor::TypecheckVisitor(shared_ptr<TypeContext> ctx,
                                   shared_ptr<vector<StmtPtr>> stmts)
    : ctx(ctx) {
  prependStmts = stmts ? stmts : make_shared<vector<StmtPtr>>();
}

PatternPtr TypecheckVisitor::transform(const PatternPtr &pat) {
  if (!pat)
    return nullptr;
  TypecheckVisitor v(ctx, prependStmts);
  v.setSrcInfo(pat->getSrcInfo());
  pat->accept(v);
  return move(v.resultPattern);
}

StmtPtr TypecheckVisitor::apply(shared_ptr<Cache> cache, StmtPtr stmts) {
  auto ctx = make_shared<TypeContext>(cache);
  TypecheckVisitor v(ctx);
  return v.realizeBlock(stmts, true);
}

void TypecheckVisitor::defaultVisit(const Pattern *p) { resultPattern = p->clone(); }

/*************************************************************************************/

void TypecheckVisitor::visit(const StarPattern *pat) {
  resultPattern = N<StarPattern>();
  resultPattern->setType(
      forceUnify(pat, ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel)));
}

void TypecheckVisitor::visit(const IntPattern *pat) {
  resultPattern = N<IntPattern>(pat->value);
  resultPattern->setType(forceUnify(pat, ctx->findInternal("int")));
}

void TypecheckVisitor::visit(const BoolPattern *pat) {
  resultPattern = N<BoolPattern>(pat->value);
  resultPattern->setType(forceUnify(pat, ctx->findInternal("bool")));
}

void TypecheckVisitor::visit(const StrPattern *pat) {
  resultPattern = N<StrPattern>(pat->value, pat->prefix);
  if (pat->prefix == "s")
    resultPattern->setType(forceUnify(pat, ctx->findInternal("seq")));
  else
    resultPattern->setType(forceUnify(pat, ctx->findInternal("str")));
}

void TypecheckVisitor::visit(const RangePattern *pat) {
  resultPattern = N<RangePattern>(pat->start, pat->stop);
  resultPattern->setType(forceUnify(pat, ctx->findInternal("int")));
}

void TypecheckVisitor::visit(const TuplePattern *pat) {
  auto p = N<TuplePattern>(transform(pat->patterns));
  vector<TypePtr> types;
  for (auto &pp : p->patterns)
    types.push_back(pp->getType());
  auto t = ctx->instantiateGeneric(
      getSrcInfo(), ctx->findInternal(format("Tuple.N{}", types.size())), {types});
  resultPattern = move(p);
  resultPattern->setType(forceUnify(pat, t));
}

void TypecheckVisitor::visit(const ListPattern *pat) {
  auto p = N<ListPattern>(transform(pat->patterns));
  TypePtr t = ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel);
  for (auto &pp : p->patterns)
    forceUnify(t, pp->getType());
  t = ctx->instantiateGeneric(getSrcInfo(), ctx->findInternal("List"), {t});
  resultPattern = move(p);
  resultPattern->setType(forceUnify(pat, t));
}

void TypecheckVisitor::visit(const OrPattern *pat) {
  auto p = N<OrPattern>(transform(pat->patterns));
  assert(p->patterns.size());
  TypePtr t = p->patterns[0]->getType();
  for (auto &pp : p->patterns)
    forceUnify(t, pp->getType());
  resultPattern = move(p);
  resultPattern->setType(forceUnify(pat, t));
}

void TypecheckVisitor::visit(const WildcardPattern *pat) {
  resultPattern = N<WildcardPattern>(pat->var);
  auto t = forceUnify(pat, ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel));
  if (pat->var != "")
    ctx->add(TypecheckItem::Var, pat->var, t);
  resultPattern->setType(t);
}

void TypecheckVisitor::visit(const GuardedPattern *pat) {
  auto p = N<GuardedPattern>(transform(pat->pattern), transform(pat->cond));
  auto t = p->pattern->getType();
  resultPattern = move(p);
  resultPattern->setType(forceUnify(pat, t));
}

void TypecheckVisitor::visit(const BoundPattern *pat) {
  auto p = N<BoundPattern>(pat->var, transform(pat->pattern));
  auto t = p->pattern->getType();
  ctx->add(TypecheckItem::Var, p->var, t);
  resultPattern = move(p);
  resultPattern->setType(forceUnify(pat, t));
}

/*******************************/

StaticVisitor::StaticVisitor(map<string, types::Generic> &m)
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
  auto i = transform(expr->ifexpr);
  auto e = transform(expr->elsexpr);
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
  else if (expr->op == "//") {
    if (!value2)
      error("division by zero");
    value = value / value2;
  } else if (expr->op == "%") {
    if (!value2)
      error("division by zero");
    value = value % value2;
  } else
    error(expr->getSrcInfo(), "not a static binary expression");
}

} // namespace ast
} // namespace seq
