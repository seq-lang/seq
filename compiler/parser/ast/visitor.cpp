#include "parser/ast/visitor.h"
#include "parser/ast/ast.h"

namespace seq {
namespace ast {

void ASTVisitor::defaultVisit(const Expr *e) {}
void ASTVisitor::defaultVisit(const Stmt *e) {}
void ASTVisitor::defaultVisit(const Pattern *e) {}

void ASTVisitor::visit(const NoneExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const BoolExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const IntExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const FloatExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const StringExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const FStringExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const KmerExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const SeqExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const IdExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const UnpackExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const TupleExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const ListExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const SetExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const DictExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const GeneratorExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const DictGeneratorExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const IfExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const UnaryExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const BinaryExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const PipeExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const IndexExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const TupleIndexExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const CallExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const StackAllocExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const DotExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const SliceExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const EllipsisExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const TypeOfExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const PtrExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const LambdaExpr *e) { defaultVisit(e); }
void ASTVisitor::visit(const YieldExpr *e) { defaultVisit(e); }

void ASTVisitor::visit(const SuiteStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const PassStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const BreakStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const ContinueStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const ExprStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const AssignStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const AssignMemberStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const UpdateStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const DelStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const PrintStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const ReturnStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const YieldStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const AssertStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const WhileStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const ForStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const IfStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const MatchStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const ExtendStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const ImportStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const ExternImportStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const TryStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const GlobalStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const ThrowStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const FunctionStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const ClassStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const AssignEqStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const YieldFromStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const WithStmt *e) { defaultVisit(e); }
void ASTVisitor::visit(const PyDefStmt *e) { defaultVisit(e); }

void ASTVisitor::visit(const StarPattern *e) { defaultVisit(e); }
void ASTVisitor::visit(const IntPattern *e) { defaultVisit(e); }
void ASTVisitor::visit(const BoolPattern *e) { defaultVisit(e); }
void ASTVisitor::visit(const StrPattern *e) { defaultVisit(e); }
void ASTVisitor::visit(const SeqPattern *e) { defaultVisit(e); }
void ASTVisitor::visit(const RangePattern *e) { defaultVisit(e); }
void ASTVisitor::visit(const TuplePattern *e) { defaultVisit(e); }
void ASTVisitor::visit(const ListPattern *e) { defaultVisit(e); }
void ASTVisitor::visit(const OrPattern *e) { defaultVisit(e); }
void ASTVisitor::visit(const WildcardPattern *e) { defaultVisit(e); }
void ASTVisitor::visit(const GuardedPattern *e) { defaultVisit(e); }
void ASTVisitor::visit(const BoundPattern *e) { defaultVisit(e); }

} // namespace ast
} // namespace seq
