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

StmtPtr
TypecheckVisitor::apply(shared_ptr<Cache> cache, StmtPtr stmts,
                        const unordered_map<string, pair<string, seq_int_t>> &defines) {
  auto ctx = make_shared<TypeContext>(cache);
  cache->typeCtx = ctx;
  TypecheckVisitor v(ctx);
  for (auto &d : defines)
    ctx->add(TypecheckItem::Type, d.first, make_shared<StaticType>(d.second.second),
             true);
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
  a->unify(b.get(), &undo);
  ast::error(
      b->getSrcInfo(),
      fmt::format("cannot unify {} and {}", a->toString(), b->toString()).c_str());
  return nullptr;
}

} // namespace ast
} // namespace seq
