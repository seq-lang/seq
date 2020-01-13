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
#include "seq/seq.h"

class CodegenStmtVisitor;

class CodegenExprVisitor : public ExprVisitor {
  Context &ctx;
  CodegenStmtVisitor &stmtVisitor;
  seq::Expr *result;
  friend class CodegenStmtVisitor;

public:
  CodegenExprVisitor(Context &ctx, CodegenStmtVisitor &stmtVisitor);

  void Set(seq::Expr *p);
  seq::Expr *Visit(Expr &e);

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

class CodegenStmtVisitor : public StmtVisitor {
  Context &ctx;
  seq::Stmt *result;

public:
  CodegenStmtVisitor(Context &ctx);

  static void apply(Context &ctx, unique_ptr<SuiteStmt> &s);
  void Set(seq::Stmt *p);
  seq::Stmt *Visit(Stmt &e);
  seq::Expr *Visit(Expr &e);
  seq::types::Type *VisitType(Expr &e);

  virtual void visit(SuiteStmt &) override;
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
};

#define RETURN(T, ...) Set(new T(__VA_ARGS__))
