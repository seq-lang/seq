#pragma once

#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/common.h"
#include "parser/context.h"
#include "parser/expr.h"
#include "parser/stmt.h"
#include "parser/visitor.h"

class TransformStmtVisitor;

class TransformExprVisitor : public ExprVisitor {
  // Context &ctx;
  // TransformStmtVisitor &stmtVisitor;
  ExprPtr result{nullptr};
  unique_ptr<seq::SrcInfo> newSrcInfo{nullptr};
  friend class TransformStmtVisitor;

public:
  void Set(ExprPtr &&p);
  ExprPtr Visit(Expr &e);
  ExprPtr Visit(Expr &e, const seq::SrcInfo &newInfo);

  void visit(EmptyExpr &) override;
  void visit(BoolExpr &) override;
  void visit(IntExpr &) override;
  void visit(FloatExpr &) override;
  void visit(StringExpr &) override;
  void visit(FStringExpr &) override;
  void visit(KmerExpr &) override;
  void visit(SeqExpr &) override;
  void visit(IdExpr &) override;
  void visit(UnpackExpr &) override;
  void visit(TupleExpr &) override;
  void visit(ListExpr &) override;
  void visit(SetExpr &) override;
  void visit(DictExpr &) override;
  void visit(GeneratorExpr &) override;
  void visit(DictGeneratorExpr &) override;
  void visit(IfExpr &) override;
  void visit(UnaryExpr &) override;
  void visit(BinaryExpr &) override;
  void visit(PipeExpr &) override;
  void visit(IndexExpr &) override;
  void visit(CallExpr &) override;
  void visit(DotExpr &) override;
  void visit(SliceExpr &) override;
  void visit(EllipsisExpr &) override;
  void visit(TypeOfExpr &) override;
  void visit(PtrExpr &) override;
  void visit(LambdaExpr &) override;
  void visit(YieldExpr &) override;
};

class TransformStmtVisitor : public StmtVisitor {
  // Context &ctx;
  StmtPtr result{nullptr};
  unique_ptr<seq::SrcInfo> newSrcInfo{nullptr};

public:
  void Set(StmtPtr &&p);
  StmtPtr Visit(Stmt &e);
  StmtPtr Visit(Stmt &e, const seq::SrcInfo &newInfo);
  ExprPtr Visit(Expr &e);
  ExprPtr Visit(Expr &e, const seq::SrcInfo &newInfo);

  virtual void visit(PassStmt &) override;
  virtual void visit(BreakStmt &) override;
  virtual void visit(ContinueStmt &) override;
  virtual void visit(ExprStmt &) override;
  virtual void visit(AssignStmt &) override;
  virtual void visit(DelStmt &) override;
  virtual void visit(PrintStmt &) override;
  virtual void visit(ReturnStmt &) override;
  virtual void visit(YieldStmt &) override;
  virtual void visit(AssertStmt &) override;
  virtual void visit(TypeAliasStmt &) override;
  virtual void visit(WhileStmt &) override;
  virtual void visit(ForStmt &) override;
  virtual void visit(IfStmt &) override;
  virtual void visit(MatchStmt &) override;
  virtual void visit(ExtendStmt &) override;
  virtual void visit(ImportStmt &) override;
  virtual void visit(ExternImportStmt &) override;
  virtual void visit(TryStmt &) override;
  virtual void visit(GlobalStmt &) override;
  virtual void visit(ThrowStmt &) override;
  virtual void visit(PrefetchStmt &) override;
  virtual void visit(FunctionStmt &) override;
  virtual void visit(ClassStmt &) override;
  virtual void visit(DeclareStmt &) override;
};
