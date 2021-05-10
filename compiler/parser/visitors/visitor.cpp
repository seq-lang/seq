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

void ASTVisitor::defaultVisit(Expr *expr) {}
void ASTVisitor::defaultVisit(Stmt *stmt) {}

void ASTVisitor::visit(NoneExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(BoolExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(IntExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(FloatExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(StringExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(IdExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(StarExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(KeywordStarExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(TupleExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(ListExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(SetExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(DictExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(GeneratorExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(DictGeneratorExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(IfExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(UnaryExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(BinaryExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(ChainBinaryExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(PipeExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(IndexExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(TupleIndexExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(CallExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(StackAllocExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(DotExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(SliceExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(EllipsisExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(TypeOfExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(PtrExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(LambdaExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(YieldExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(AssignExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(RangeExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(InstantiateExpr *expr) { defaultVisit(expr); }
void ASTVisitor::visit(StmtExpr *expr) { defaultVisit(expr); }

void ASTVisitor::visit(SuiteStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(PassStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(BreakStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(ContinueStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(ExprStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(AssignStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(AssignMemberStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(UpdateStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(DelStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(PrintStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(ReturnStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(YieldStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(AssertStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(WhileStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(ForStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(IfStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(MatchStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(ImportStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(TryStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(GlobalStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(ThrowStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(FunctionStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(ClassStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(YieldFromStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(WithStmt *stmt) { defaultVisit(stmt); }
void ASTVisitor::visit(CustomStmt *stmt) { defaultVisit(stmt); }

} // namespace ast
} // namespace seq
