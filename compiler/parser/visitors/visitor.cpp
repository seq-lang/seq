/*
 * visitor.cpp --- Seq AST visitors.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include "parser/visitors/visitor.h"
#include "parser/ast.h"

namespace seq {
namespace ast {

void ASTVisitor::defaultVisit(const Expr *expr) {}
void ASTVisitor::defaultVisit(const Stmt *stmt) {}
void ASTVisitor::defaultVisit(const Pattern *pattern) {}

void ASTVisitor::visit(const NoneExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const BoolExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const IntExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const FloatExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const StringExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const IdExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const StarExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const TupleExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const ListExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const SetExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const DictExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const GeneratorExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const DictGeneratorExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const IfExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const UnaryExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const BinaryExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const PipeExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const IndexExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const TupleIndexExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const CallExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const StackAllocExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const DotExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const SliceExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const EllipsisExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const TypeOfExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const PtrExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const LambdaExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const YieldExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const InstantiateExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const StaticExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(const StmtExpr *expr) { defaultVisit(expr); }

void ASTVisitor::visit(const SuiteStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(const PassStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(const BreakStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(const ContinueStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(const ExprStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(const AssignStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(const AssignMemberStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(const UpdateStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(const DelStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(const PrintStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(const ReturnStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(const YieldStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(const AssertStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(const WhileStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(const ForStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(const IfStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(const MatchStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(const ImportStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(const TryStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(const GlobalStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(const ThrowStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(const FunctionStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(const ClassStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(const YieldFromStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(const WithStmt *stmt) { defaultVisit(stmt); }

void ASTVisitor::visit(const StarPattern *pattern) { defaultVisit(pattern); }
void ASTVisitor::visit(const IntPattern *pattern) { defaultVisit(pattern); }
void ASTVisitor::visit(const BoolPattern *pattern) { defaultVisit(pattern); }
void ASTVisitor::visit(const StrPattern *pattern) { defaultVisit(pattern); }
void ASTVisitor::visit(const RangePattern *pattern) { defaultVisit(pattern); }
void ASTVisitor::visit(const TuplePattern *pattern) { defaultVisit(pattern); }
void ASTVisitor::visit(const ListPattern *pattern) { defaultVisit(pattern); }
void ASTVisitor::visit(const OrPattern *pattern) { defaultVisit(pattern); }
void ASTVisitor::visit(const WildcardPattern *pattern) { defaultVisit(pattern); }
void ASTVisitor::visit(const GuardedPattern *pattern) { defaultVisit(pattern); }
void ASTVisitor::visit(const BoundPattern *pattern) { defaultVisit(pattern); }

} // namespace ast
} // namespace seq
