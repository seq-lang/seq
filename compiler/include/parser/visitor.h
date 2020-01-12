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
  virtual void visit(EmptyExpr &) = 0;
  virtual void visit(BoolExpr &) = 0;
  virtual void visit(IntExpr &) = 0;
  virtual void visit(FloatExpr &) = 0;
  virtual void visit(StringExpr &) = 0;
  virtual void visit(FStringExpr &) = 0;
  virtual void visit(KmerExpr &) = 0;
  virtual void visit(SeqExpr &) = 0;
  virtual void visit(IdExpr &) = 0;
  virtual void visit(UnpackExpr &) = 0;
  virtual void visit(TupleExpr &) = 0;
  virtual void visit(ListExpr &) = 0;
  virtual void visit(SetExpr &) = 0;
  virtual void visit(DictExpr &) = 0;
  virtual void visit(GeneratorExpr &) = 0;
  virtual void visit(DictGeneratorExpr &) = 0;
  virtual void visit(IfExpr &) = 0;
  virtual void visit(UnaryExpr &) = 0;
  virtual void visit(BinaryExpr &) = 0;
  virtual void visit(PipeExpr &) = 0;
  virtual void visit(IndexExpr &) = 0;
  virtual void visit(CallExpr &) = 0;
  virtual void visit(DotExpr &) = 0;
  virtual void visit(SliceExpr &) = 0;
  virtual void visit(EllipsisExpr &) = 0;
  virtual void visit(TypeOfExpr &) = 0;
  virtual void visit(PtrExpr &) = 0;
  virtual void visit(LambdaExpr &) = 0;
  virtual void visit(YieldExpr &) = 0;
};

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
  virtual void visit(PassStmt &) {};
  virtual void visit(BreakStmt &) {};
  virtual void visit(ContinueStmt &) {};
  virtual void visit(ExprStmt &) {};
  virtual void visit(AssignStmt &) {};
  virtual void visit(DelStmt &) {};
  virtual void visit(PrintStmt &) {};
  virtual void visit(ReturnStmt &) {};
  virtual void visit(YieldStmt &) {};
  virtual void visit(AssertStmt &) {};
  virtual void visit(TypeAliasStmt &) {};
  virtual void visit(WhileStmt &) {};
  virtual void visit(ForStmt &) {};
  virtual void visit(IfStmt &) {}
  virtual void visit(MatchStmt &) {}
  virtual void visit(ExtendStmt &) {}
  virtual void visit(ImportStmt &) {}
  virtual void visit(ExternImportStmt &) {}
  virtual void visit(TryStmt &) {}
  virtual void visit(GlobalStmt &) {}
  virtual void visit(ThrowStmt &) {}
  virtual void visit(PrefetchStmt &) {}
  virtual void visit(FunctionStmt &) {}
  virtual void visit(ClassStmt &) {}
  virtual void visit(DeclareStmt &) {}
};
