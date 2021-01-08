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

#include "parser/ast.h"
#include "parser/cache.h"
#include "parser/common.h"
#include "parser/visitors/codegen/codegen_ctx.h"
#include "parser/visitors/visitor.h"

#include "sir/sir.h"

namespace seq {
namespace ast {

class CodegenVisitor : public CallbackASTVisitor<seq::ir::ValuePtr, seq::ir::ValuePtr> {
  shared_ptr<CodegenContext> ctx;
  seq::ir::ValuePtr result;
  const seq::ir::types::Type *typeResult = nullptr;

  void defaultVisit(Expr *expr) override;
  void defaultVisit(Stmt *expr) override;

  seq::ir::Func *realizeFunc(const string &name);
  shared_ptr<CodegenItem> processIdentifier(shared_ptr<CodegenContext> tctx,
                                            const string &id);

public:
  explicit CodegenVisitor(shared_ptr<CodegenContext> ctx);
  static seq::ir::IRModulePtr apply(shared_ptr<Cache> cache, StmtPtr stmts);

  seq::ir::ValuePtr transform(const ExprPtr &expr) override;
  seq::ir::ValuePtr transform(const StmtPtr &stmt) override;

  seq::ir::types::Type *realizeType(types::ClassType *t);

public:
  void visit(BoolExpr *) override;
  void visit(IntExpr *) override;
  void visit(FloatExpr *) override;
  void visit(StringExpr *) override;
  void visit(IdExpr *) override;
  void visit(IfExpr *) override;
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

private:
  std::unique_ptr<ir::SeriesFlow> newScope(const seq::SrcObject *s, std::string name);

  template <typename T> T *cast(seq::ir::IRNode *obj) {
    if (obj)
      return obj->as<T>();
    else
      return nullptr;
  }

  template <typename T, typename V> T *cast(const std::unique_ptr<V> &obj) {
    if (obj)
      return obj->template as<T>();
    else
      return nullptr;
  }

  template <typename T> auto wrap(T *obj) { return std::unique_ptr<T>(obj); }
};

} // namespace ast
} // namespace seq
