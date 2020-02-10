#pragma once

namespace seq {
namespace ast {

struct EmptyExpr;
struct BoolExpr;
struct IntExpr;
struct FloatExpr;
struct StringExpr;
struct FStringExpr;
struct KmerExpr;
struct SeqExpr;
struct IdExpr;
struct UnpackExpr;
struct TupleExpr;
struct ListExpr;
struct SetExpr;
struct DictExpr;
struct GeneratorExpr;
struct DictGeneratorExpr;
struct IfExpr;
struct UnaryExpr;
struct BinaryExpr;
struct PipeExpr;
struct IndexExpr;
struct CallExpr;
struct DotExpr;
struct SliceExpr;
struct EllipsisExpr;
struct TypeOfExpr;
struct PtrExpr;
struct LambdaExpr;
struct YieldExpr;

struct ExprVisitor {
  virtual void visit(const EmptyExpr *) = 0;
  virtual void visit(const BoolExpr *) = 0;
  virtual void visit(const IntExpr *) = 0;
  virtual void visit(const FloatExpr *) = 0;
  virtual void visit(const StringExpr *) = 0;
  virtual void visit(const FStringExpr *) = 0;
  virtual void visit(const KmerExpr *) = 0;
  virtual void visit(const SeqExpr *) = 0;
  virtual void visit(const IdExpr *) = 0;
  virtual void visit(const UnpackExpr *) = 0;
  virtual void visit(const TupleExpr *) = 0;
  virtual void visit(const ListExpr *) = 0;
  virtual void visit(const SetExpr *) = 0;
  virtual void visit(const DictExpr *) = 0;
  virtual void visit(const GeneratorExpr *) = 0;
  virtual void visit(const DictGeneratorExpr *) = 0;
  virtual void visit(const IfExpr *) = 0;
  virtual void visit(const UnaryExpr *) = 0;
  virtual void visit(const BinaryExpr *) = 0;
  virtual void visit(const PipeExpr *) = 0;
  virtual void visit(const IndexExpr *) = 0;
  virtual void visit(const CallExpr *) = 0;
  virtual void visit(const DotExpr *) = 0;
  virtual void visit(const SliceExpr *) = 0;
  virtual void visit(const EllipsisExpr *) = 0;
  virtual void visit(const TypeOfExpr *) = 0;
  virtual void visit(const PtrExpr *) = 0;
  virtual void visit(const LambdaExpr *) = 0;
  virtual void visit(const YieldExpr *) = 0;
};

struct SuiteStmt;
struct PassStmt;
struct BreakStmt;
struct ContinueStmt;
struct ExprStmt;
struct AssignStmt;
struct DelStmt;
struct PrintStmt;
struct ReturnStmt;
struct YieldStmt;
struct AssertStmt;
struct TypeAliasStmt;
struct WhileStmt;
struct ForStmt;
struct IfStmt;
struct MatchStmt;
struct ExtendStmt;
struct ImportStmt;
struct ExternImportStmt;
struct TryStmt;
struct GlobalStmt;
struct ThrowStmt;
struct FunctionStmt;
struct ClassStmt;
struct DeclareStmt;
struct AssignEqStmt;
struct YieldFromStmt;
struct WithStmt;
struct PyDefStmt;

struct StmtVisitor {
  virtual void visit(const SuiteStmt *) = 0;
  virtual void visit(const PassStmt *) = 0;
  virtual void visit(const BreakStmt *) = 0;
  virtual void visit(const ContinueStmt *) = 0;
  virtual void visit(const ExprStmt *) = 0;
  virtual void visit(const AssignStmt *) = 0;
  virtual void visit(const DelStmt *) = 0;
  virtual void visit(const PrintStmt *) = 0;
  virtual void visit(const ReturnStmt *) = 0;
  virtual void visit(const YieldStmt *) = 0;
  virtual void visit(const AssertStmt *) = 0;
  virtual void visit(const TypeAliasStmt *) = 0;
  virtual void visit(const WhileStmt *) = 0;
  virtual void visit(const ForStmt *) = 0;
  virtual void visit(const IfStmt *) = 0;
  virtual void visit(const MatchStmt *) = 0;
  virtual void visit(const ExtendStmt *) = 0;
  virtual void visit(const ImportStmt *) = 0;
  virtual void visit(const ExternImportStmt *) = 0;
  virtual void visit(const TryStmt *) = 0;
  virtual void visit(const GlobalStmt *) = 0;
  virtual void visit(const ThrowStmt *) = 0;
  virtual void visit(const FunctionStmt *) = 0;
  virtual void visit(const ClassStmt *) = 0;
  virtual void visit(const DeclareStmt *) = 0;
  virtual void visit(const AssignEqStmt *) = 0;
  virtual void visit(const YieldFromStmt *) = 0;
  virtual void visit(const WithStmt *) = 0;
  virtual void visit(const PyDefStmt *) = 0;
};

struct StarPattern;
struct IntPattern;
struct BoolPattern;
struct StrPattern;
struct SeqPattern;
struct RangePattern;
struct TuplePattern;
struct ListPattern;
struct OrPattern;
struct WildcardPattern;
struct GuardedPattern;
struct BoundPattern;

struct PatternVisitor {
  virtual void visit(const StarPattern *) = 0;
  virtual void visit(const IntPattern *) = 0;
  virtual void visit(const BoolPattern *) = 0;
  virtual void visit(const StrPattern *) = 0;
  virtual void visit(const SeqPattern *) = 0;
  virtual void visit(const RangePattern *) = 0;
  virtual void visit(const TuplePattern *) = 0;
  virtual void visit(const ListPattern *) = 0;
  virtual void visit(const OrPattern *) = 0;
  virtual void visit(const WildcardPattern *) = 0;
  virtual void visit(const GuardedPattern *) = 0;
  virtual void visit(const BoundPattern *) = 0;
};

} // namespace ast
} // namespace seq
