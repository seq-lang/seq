#pragma once

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
struct PrefetchStmt;
struct FunctionStmt;
struct ClassStmt;
struct DeclareStmt;

struct StmtVisitor {
  virtual void visit(const SuiteStmt *) {};
  virtual void visit(const PassStmt *) {};
  virtual void visit(const BreakStmt *) {};
  virtual void visit(const ContinueStmt *) {};
  virtual void visit(const ExprStmt *) {};
  virtual void visit(const AssignStmt *) {};
  virtual void visit(const DelStmt *) {};
  virtual void visit(const PrintStmt *) {};
  virtual void visit(const ReturnStmt *) {};
  virtual void visit(const YieldStmt *) {};
  virtual void visit(const AssertStmt *) {};
  virtual void visit(const TypeAliasStmt *) {};
  virtual void visit(const WhileStmt *) {};
  virtual void visit(const ForStmt *) {};
  virtual void visit(const IfStmt *) {}
  virtual void visit(const MatchStmt *) {}
  virtual void visit(const ExtendStmt *) {}
  virtual void visit(const ImportStmt *) {}
  virtual void visit(const ExternImportStmt *) {}
  virtual void visit(const TryStmt *) {}
  virtual void visit(const GlobalStmt *) {}
  virtual void visit(const ThrowStmt *) {}
  virtual void visit(const PrefetchStmt *) {}
  virtual void visit(const FunctionStmt *) {}
  virtual void visit(const ClassStmt *) {}
  virtual void visit(const DeclareStmt *) {}
};
