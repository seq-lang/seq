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

StmtPtr TransformVisitor::transform(const Stmt *stmt) {
  if (!stmt)
    return nullptr;

  TransformVisitor v(ctx);
  v.setSrcInfo(stmt->getSrcInfo());

  stmt->accept(v);
  if (v.prependStmts->size()) {
    if (v.resultStmt)
      v.prependStmts->push_back(move(v.resultStmt));
    v.resultStmt = N<SuiteStmt>(move(*v.prependStmts));
  }
  return move(v.resultStmt);
}

void TransformVisitor::visit(const SuiteStmt *stmt) {
  vector<StmtPtr> r;
  for (auto &s : stmt->stmts)
    if (auto t = transform(s))
      r.push_back(move(t));
  resultStmt = N<SuiteStmt>(move(r));
}

void TransformVisitor::visit(const PassStmt *stmt) { resultStmt = N<PassStmt>(); }

void TransformVisitor::visit(const BreakStmt *stmt) { resultStmt = N<BreakStmt>(); }

void TransformVisitor::visit(const ContinueStmt *stmt) {
  resultStmt = N<ContinueStmt>();
}

// Transformation
void TransformVisitor::visit(const AssignEqStmt *stmt) {
  resultStmt = transform(N<AssignStmt>(
      stmt->lhs->clone(),
      N<BinaryExpr>(stmt->lhs->clone(), stmt->op, stmt->rhs->clone(), true), nullptr,
      true));
}

// Transformation
void TransformVisitor::visit(const YieldFromStmt *stmt) {
  auto var = getTemporaryVar("yield");
  resultStmt = transform(
      N<ForStmt>(N<IdExpr>(var), stmt->expr->clone(), N<YieldStmt>(N<IdExpr>(var))));
}

// Transformation
void TransformVisitor::visit(const WithStmt *stmt) {
  assert(stmt->items.size());
  vector<StmtPtr> content;
  for (int i = stmt->items.size() - 1; i >= 0; i--) {
    vector<StmtPtr> internals;
    string var = stmt->vars[i] == "" ? getTemporaryVar("with") : stmt->vars[i];
    internals.push_back(N<AssignStmt>(N<IdExpr>(var), stmt->items[i]->clone()));
    internals.push_back(
        N<ExprStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "__enter__"))));
    internals.push_back(
        N<TryStmt>(content.size() ? N<SuiteStmt>(move(content)) : stmt->suite->clone(),
                   vector<TryStmt::Catch>{},
                   N<SuiteStmt>(N<ExprStmt>(
                       N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "__exit__"))))));
    content = move(internals);
  }
  resultStmt = transform(N<IfStmt>(N<BoolExpr>(true), N<SuiteStmt>(move(content))));
}

// Transformation
void TransformVisitor::visit(const PyDefStmt *stmt) {
  // _py_exec(""" str """)
  vector<string> args;
  for (auto &a : stmt->args)
    args.push_back(a.name);
  string code =
      format("def {}({}):\n{}\n", stmt->name, fmt::join(args, ", "), stmt->code);
  resultStmt = transform(
      N<SuiteStmt>(N<ExprStmt>(N<CallExpr>(N<IdExpr>("_py_exec"), N<StringExpr>(code))),
                   // from __main__ pyimport foo () -> ret
                   N<ExternImportStmt>(make_pair(stmt->name, ""), N<IdExpr>("__main__"),
                                       stmt->ret->clone(), vector<Param>(), "py")));
}

} // namespace ast
} // namespace seq
