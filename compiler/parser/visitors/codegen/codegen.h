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

class CodegenVisitor : public CallbackASTVisitor<seq::ir::ValuePtr, seq::ir::ValuePtr,
                                                 seq::ir::ValuePtr> {
  shared_ptr<CodegenContext> ctx;
  seq::ir::ValuePtr result;

  void defaultVisit(const Expr *expr) override;
  void defaultVisit(const Stmt *expr) override;
  void defaultVisit(const Pattern *expr) override;

  seq::ir::Func *realizeFunc(const string &name);
  shared_ptr<CodegenItem> processIdentifier(shared_ptr<CodegenContext> tctx,
                                            const string &id);

public:
  explicit CodegenVisitor(shared_ptr<CodegenContext> ctx);
  static seq::ir::IRModulePtr apply(shared_ptr<Cache> cache, StmtPtr stmts);

  seq::ir::ValuePtr transform(const ExprPtr &expr) override;
  seq::ir::ValuePtr transform(const StmtPtr &stmt) override;
  seq::ir::ValuePtr transform(const PatternPtr &pat) override { assert(false); }

  seq::ir::types::Type *realizeType(types::ClassTypePtr t);

public:
  void visit(const BoolExpr *) override;
  void visit(const IntExpr *) override;
  void visit(const FloatExpr *) override;
  void visit(const StringExpr *) override;
  void visit(const IdExpr *) override;
  void visit(const IfExpr *) override;
  void visit(const CallExpr *) override;
  void visit(const StackAllocExpr *) override;
  void visit(const DotExpr *) override;
  void visit(const PtrExpr *) override;
  void visit(const YieldExpr *) override;
  void visit(const StmtExpr *) override;

  void visit(const SuiteStmt *) override;
  void visit(const PassStmt *) override;
  void visit(const BreakStmt *) override;
  void visit(const ContinueStmt *) override;
  void visit(const ExprStmt *) override;
  void visit(const AssignStmt *) override;
  void visit(const AssignMemberStmt *) override;
  void visit(const DelStmt *) override;
  void visit(const ReturnStmt *) override;
  void visit(const YieldStmt *) override;
  void visit(const AssertStmt *) override;
  void visit(const WhileStmt *) override;
  void visit(const ForStmt *) override;
  void visit(const IfStmt *) override;
  void visit(const UpdateStmt *) override;
  void visit(const TryStmt *) override;
  void visit(const ThrowStmt *) override;
  void visit(const FunctionStmt *) override;
  void visit(const ClassStmt *stmt) override;

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

  ir::ValuePtr stripLoad(ir::ValuePtr outer);

  template <typename T> auto wrap(T *obj) { return std::unique_ptr<T>(obj); }
};

} // namespace ast
} // namespace seq
