/**
 * TODO : Redo error messages (right now they are awful)
 */

#include "util/fmt/format.h"
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
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
                                   const shared_ptr<vector<StmtPtr>> &stmts)
    : ctx(move(ctx)) {
  prependStmts = stmts ? stmts : make_shared<vector<StmtPtr>>();
}

StmtPtr TypecheckVisitor::apply(shared_ptr<Cache> cache, StmtPtr stmts) {
  auto ctx = make_shared<TypeContext>(cache);
  TypecheckVisitor v(ctx);
  auto infer = v.inferTypes(stmts->clone(), true);
  LOG_TYPECHECK("toplevel type inference done in {} iterations", infer.first);
  return move(infer.second);
}

TypePtr operator|=(TypePtr &a, const TypePtr &b) {
  if (!a)
    return a = b;
  seqassert(b, "rhs is nullptr");
  types::Type::Unification undo;
  if (a->unify(b.get(), &undo) >= 0)
    return a;
  undo.undo();
  ast::error(
      a->getSrcInfo(),
      fmt::format("cannot unify {} and {}", a->toString(), b->toString()).c_str());
  return nullptr;
}

/**************************************************************************************/
// TODO: remove once MatchStmt is handled in SimplifyVisitor

PatternPtr TypecheckVisitor::transform(const PatternPtr &pat_) {
  if (!pat_)
    return nullptr;
  auto &pat = const_cast<PatternPtr &>(pat_);
  TypecheckVisitor v(ctx, prependStmts);
  v.setSrcInfo(pat->getSrcInfo());
  pat->accept(v);
  return move(pat);
}
void TypecheckVisitor::defaultVisit(Pattern *e) {
  seqassert(false, "unexpected AST node {}", e->toString());
}
void TypecheckVisitor::visit(StarPattern *pat) {
  pat->type |= ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel);
  pat->done = realizeType(pat->type) != nullptr;
}
void TypecheckVisitor::visit(IntPattern *pat) {
  pat->type |= ctx->findInternal("int");
  pat->done = true;
}
void TypecheckVisitor::visit(BoolPattern *pat) {
  pat->type |= ctx->findInternal("bool");
  pat->done = true;
}
void TypecheckVisitor::visit(StrPattern *pat) {
  pat->type |= ctx->findInternal(pat->prefix == "s" ? "seq" : "str");
  pat->done = true;
}
void TypecheckVisitor::visit(RangePattern *pat) {
  pat->type |= ctx->findInternal("int");
  pat->done = true;
}
void TypecheckVisitor::visit(TuplePattern *pat) {
  pat->patterns = transform(pat->patterns);
  pat->done = true;
  vector<TypePtr> types;
  for (const auto &p : pat->patterns) {
    types.push_back(p->getType());
    pat->done &= p->done;
  }
  pat->type |= ctx->instantiateGeneric(
      getSrcInfo(), ctx->findInternal(format("Tuple.N{}", types.size())), {types});
}
void TypecheckVisitor::visit(ListPattern *pat) {
  pat->patterns = transform(pat->patterns);
  pat->done = true;
  TypePtr typ = ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel);
  for (const auto &p : pat->patterns) {
    typ |= p->type;
    pat->done &= p->done;
  }
  pat->type |= ctx->instantiateGeneric(getSrcInfo(), ctx->findInternal("List"), {typ});
}
void TypecheckVisitor::visit(OrPattern *pat) {
  pat->patterns = transform(pat->patterns);
  pat->done = true;
  TypePtr typ = ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel);
  for (const auto &p : pat->patterns) {
    typ |= p->type;
    pat->done &= p->done;
  }
  pat->type |= typ;
}
void TypecheckVisitor::visit(WildcardPattern *pat) {
  pat->type |= ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel);
  if (!pat->var.empty())
    ctx->add(TypecheckItem::Var, pat->var, pat->type);
  pat->done = realizeType(pat->type) != nullptr;
}
void TypecheckVisitor::visit(GuardedPattern *pat) {
  pat->pattern = transform(pat->pattern);
  pat->cond = transform(pat->cond);
  pat->type |= pat->pattern->type;
  pat->done = pat->pattern->done && pat->cond->done;
}
void TypecheckVisitor::visit(BoundPattern *pat) {
  pat->pattern = transform(pat->pattern);
  pat->type |= pat->pattern->type;
  pat->done = pat->pattern->done;
  ctx->add(TypecheckItem::Var, pat->var, pat->type);
}

} // namespace ast
} // namespace seq
