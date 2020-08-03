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
PatternPtr TransformVisitor::transform(const Pattern *pat) {
  if (!pat)
    return nullptr;
  TransformVisitor v(ctx, prependStmts);
  v.setSrcInfo(pat->getSrcInfo());
  pat->accept(v);
  return move(v.resultPattern);
}

void TransformVisitor::visit(const StarPattern *pat) {
  resultPattern = N<StarPattern>();
  if (ctx->isTypeChecking())
    resultPattern->setType(forceUnify(
        pat, forceUnify(ctx->getMatchType(), ctx->addUnbound(getSrcInfo()))));
}

void TransformVisitor::visit(const IntPattern *pat) {
  resultPattern = N<IntPattern>(pat->value);
  if (ctx->isTypeChecking())
    resultPattern->setType(forceUnify(
        pat, forceUnify(ctx->getMatchType(), ctx->findInternal("int"))));
}

void TransformVisitor::visit(const BoolPattern *pat) {
  resultPattern = N<BoolPattern>(pat->value);
  if (ctx->isTypeChecking())
    resultPattern->setType(forceUnify(
        pat, forceUnify(ctx->getMatchType(), ctx->findInternal("bool"))));
}

void TransformVisitor::visit(const StrPattern *pat) {
  resultPattern = N<StrPattern>(pat->value);
  if (ctx->isTypeChecking())
    resultPattern->setType(forceUnify(
        pat, forceUnify(ctx->getMatchType(), ctx->findInternal("str"))));
}

void TransformVisitor::visit(const SeqPattern *pat) {
  resultPattern = N<SeqPattern>(pat->value);
  if (ctx->isTypeChecking())
    resultPattern->setType(forceUnify(
        pat, forceUnify(ctx->getMatchType(), ctx->findInternal("seq"))));
}

void TransformVisitor::visit(const RangePattern *pat) {
  resultPattern = N<RangePattern>(pat->start, pat->end);
  if (ctx->isTypeChecking())
    resultPattern->setType(forceUnify(
        pat, forceUnify(ctx->getMatchType(), ctx->findInternal("int"))));
}

void TransformVisitor::visit(const TuplePattern *pat) {
  auto p = N<TuplePattern>(transform(pat->patterns));
  TypePtr t = nullptr;
  if (ctx->isTypeChecking()) {
    vector<TypePtr> types;
    for (auto &pp : p->patterns)
      types.push_back(pp->getType());
    // TODO: Ensure type...
    error("not yet implemented");
    t = make_shared<ClassType>("tuple", true, types);
  }
  resultPattern = move(p);
  if (ctx->isTypeChecking())
    resultPattern->setType(forceUnify(pat, forceUnify(ctx->getMatchType(), t)));
}

void TransformVisitor::visit(const ListPattern *pat) {
  auto p = N<ListPattern>(transform(pat->patterns));
  TypePtr t = nullptr;
  if (ctx->isTypeChecking()) {
    TypePtr ty = ctx->addUnbound(getSrcInfo());
    for (auto &pp : p->patterns)
      forceUnify(ty, pp->getType());
    t = ctx->instantiateGeneric(getSrcInfo(), ctx->findInternal("list"), {ty});
  }
  resultPattern = move(p);
  if (ctx->isTypeChecking())
    resultPattern->setType(forceUnify(pat, forceUnify(ctx->getMatchType(), t)));
}

void TransformVisitor::visit(const OrPattern *pat) {
  auto p = N<OrPattern>(transform(pat->patterns));
  assert(p->patterns.size());
  TypePtr t = nullptr;
  if (ctx->isTypeChecking()) {
    t = p->patterns[0]->getType();
    for (auto &pp : p->patterns)
      forceUnify(t, pp->getType());
  }
  resultPattern = move(p);
  if (ctx->isTypeChecking())
    resultPattern->setType(forceUnify(pat, forceUnify(ctx->getMatchType(), t)));
}

void TransformVisitor::visit(const WildcardPattern *pat) {
  resultPattern = N<WildcardPattern>(pat->var);
  if (pat->var != "")
    ctx->addVar(pat->var, ctx->getMatchType());
  if (ctx->isTypeChecking())
    resultPattern->setType(forceUnify(pat, ctx->getMatchType()));
}

void TransformVisitor::visit(const GuardedPattern *pat) {
  auto p = N<GuardedPattern>(transform(pat->pattern), makeBoolExpr(pat->cond));
  auto t = p->pattern->getType();
  resultPattern = move(p);
  if (ctx->isTypeChecking())
    resultPattern->setType(forceUnify(pat, forceUnify(ctx->getMatchType(), t)));
}

void TransformVisitor::visit(const BoundPattern *pat) {
  auto p = N<BoundPattern>(pat->var, transform(pat->pattern));
  auto t = p->pattern->getType();
  ctx->addVar(p->var, t);
  resultPattern = move(p);
  if (ctx->isTypeChecking())
    resultPattern->setType(forceUnify(pat, forceUnify(ctx->getMatchType(), t)));
}

} // namespace ast
} // namespace seq
