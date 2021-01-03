/**
 * codegen.h
 * Code generation AST walker.
 *
 * Transforms a given AST to a Seq LLVM AST.
 */

#pragma once

#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "lang/seq.h"
#include "parser/ast.h"
#include "parser/cache.h"
#include "parser/common.h"
#include "parser/visitors/codegen/codegen_ctx.h"
#include "parser/visitors/visitor.h"

namespace seq {
namespace ast {

class CodegenVisitor : public CallbackASTVisitor<seq::Expr *, seq::Stmt *> {
  shared_ptr<CodegenContext> ctx;
  seq::Expr *resultExpr;
  seq::Stmt *resultStmt;

  void defaultVisit(Expr *expr) override;
  void defaultVisit(Stmt *expr) override;

  seq::types::Type *realizeType(types::ClassType *t);
  seq::BaseFunc *realizeFunc(const string &name);
  shared_ptr<CodegenItem> processIdentifier(shared_ptr<CodegenContext> tctx,
                                            const string &id);

public:
  CodegenVisitor(shared_ptr<CodegenContext> ctx);
  static seq::SeqModule *apply(shared_ptr<Cache> cache, StmtPtr stmts);

  seq::Expr *transform(const ExprPtr &expr) override;
  seq::Stmt *transform(const StmtPtr &stmt) override;
  seq::Stmt *transform(const StmtPtr &stmt, bool addToBlock);

  void visitMethods(const string &name);

public:
  void visit(BoolExpr *) override;
  void visit(IntExpr *) override;
  void visit(FloatExpr *) override;
  void visit(StringExpr *) override;
  void visit(IdExpr *) override;
  void visit(IfExpr *) override;
  void visit(BinaryExpr *) override;
  void visit(PipeExpr *) override;
  void visit(CallExpr *) override;
  void visit(StackAllocExpr *) override;
  void visit(DotExpr *) override;
  void visit(PtrExpr *) override;
  void visit(YieldExpr *) override;
  void visit(StmtExpr *) override;

  void visit(SuiteStmt *) override;
  void visit(PassStmt *) override;
  void visit(BreakStmt *) override;
  void visit(ContinueStmt *) override;
  void visit(ExprStmt *) override;
  void visit(AssignStmt *) override;
  void visit(AssignMemberStmt *) override;
  void visit(DelStmt *) override;
  void visit(ReturnStmt *) override;
  void visit(YieldStmt *) override;
  void visit(WhileStmt *) override;
  void visit(ForStmt *) override;
  void visit(IfStmt *) override;
  void visit(UpdateStmt *) override;
  void visit(TryStmt *) override;
  void visit(ThrowStmt *) override;
  void visit(FunctionStmt *) override;
  void visit(ClassStmt *stmt) override;
};

} // namespace ast
} // namespace seq
