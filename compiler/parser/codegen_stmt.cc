#include <fmt/format.h>
#include <fmt/ostream.h>
#include <memory>
#include <ostream>
#include <stack>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/codegen.h"
#include "parser/common.h"
#include "parser/context.h"
#include "parser/expr.h"
#include "parser/stmt.h"
#include "parser/visitor.h"
#include "seq/seq.h"

using fmt::format;
using std::get;
using std::move;
using std::ostream;
using std::stack;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::unordered_set;
using std::vector;

CodegenStmtVisitor::CodegenStmtVisitor(Context &ctx) : ctx(ctx) {}

void CodegenStmtVisitor::Set(seq::Stmt *stmt) { result = stmt; }
seq::Stmt *CodegenStmtVisitor::Visit(Stmt &stmt) {
  CodegenStmtVisitor v(ctx);
  stmt.accept(v);
  if (v.result) {
    v.result->setSrcInfo(stmt.getSrcInfo());
    v.result->setBase(ctx.getBase());
    ctx.getBlock()->add(v.result);
  }
  return move(v.result);
}
seq::Expr *CodegenStmtVisitor::Visit(Expr &expr) {
  CodegenExprVisitor v(ctx, *this);
  expr.accept(v);
  return move(v.result);
}

void CodegenStmtVisitor::visit(PassStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(BreakStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(ContinueStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(ExprStmt &stmt) {
  Return(seq::ExprStmt, Visit(*stmt.expr));
}
void CodegenStmtVisitor::visit(AssignStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(DelStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(PrintStmt &stmt) {
  assert(stmt.items.size() == 1);
  Return(seq::Print, Visit(*stmt.items[0]));
}
void CodegenStmtVisitor::visit(ReturnStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(YieldStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(AssertStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(TypeAliasStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(WhileStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(ForStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(IfStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(MatchStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(ExtendStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(ImportStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(ExternImportStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(TryStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(GlobalStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(ThrowStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(PrefetchStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(FunctionStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(ClassStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}