/*
 * simplify_pattern.cpp --- AST pattern simplifications.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include "parser/ast.h"
#include "parser/common.h"
#include "parser/visitors/simplify/simplify.h"

namespace seq {
namespace ast {

PatternPtr SimplifyVisitor::transform(const PatternPtr &pat) {
  if (!pat)
    return nullptr;
  SimplifyVisitor v(ctx, preambleStmts, prependStmts);
  v.setSrcInfo(pat->getSrcInfo());
  pat->accept(v);
  return move(v.resultPattern);
}

void SimplifyVisitor::defaultVisit(const Pattern *p) { resultPattern = p->clone(); }

void SimplifyVisitor::visit(const TuplePattern *pat) {
  resultPattern = N<TuplePattern>(transform(pat->patterns));
}

void SimplifyVisitor::visit(const ListPattern *pat) {
  resultPattern = N<ListPattern>(transform(pat->patterns));
}

void SimplifyVisitor::visit(const OrPattern *pat) {
  resultPattern = N<OrPattern>(transform(pat->patterns));
}

void SimplifyVisitor::visit(const WildcardPattern *pat) {
  resultPattern = N<WildcardPattern>(pat->var);
  if (!pat->var.empty())
    ctx->add(SimplifyItem::Var, pat->var);
}

/// Transform case pattern if cond to:
///   case pattern if cond.__bool__()
void SimplifyVisitor::visit(const GuardedPattern *pat) {
  resultPattern = N<GuardedPattern>(
      transform(pat->pattern),
      transform(N<CallExpr>(N<DotExpr>(clone(pat->cond), "__bool__"))));
}

void SimplifyVisitor::visit(const BoundPattern *pat) {
  resultPattern = N<BoundPattern>(pat->var, transform(pat->pattern));
  ctx->add(SimplifyItem::Var, pat->var);
}

} // namespace ast
} // namespace seq
