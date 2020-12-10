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

#include "parser/ast/ast/ast.h"
#include "parser/ast/cache.h"
#include "parser/ast/codegen/codegen_ctx.h"
#include "parser/common.h"

#include "sir/sir.h"

namespace seq {
namespace ast {

struct CodegenResult {
  enum { OP, RVALUE, LVALUE, TYPE, NONE } tag;
  seq::ir::OperandPtr operandResult;
  seq::ir::RvaluePtr rvalueResult;
  seq::ir::LvaluePtr lvalueResult;
  seq::ir::types::Type *typeResult = nullptr;

  seq::ir::types::Type *typeOverride = nullptr;

  CodegenResult() : tag(NONE) {}
  explicit CodegenResult(seq::ir::OperandPtr op)
      : tag(OP), operandResult(std::move(op)) {}
  explicit CodegenResult(seq::ir::RvaluePtr rval)
      : tag(RVALUE), rvalueResult(std::move(rval)) {}
  explicit CodegenResult(seq::ir::LvaluePtr lval)
      : tag(LVALUE), lvalueResult(std::move(lval)) {}
  explicit CodegenResult(seq::ir::types::Type *type) : tag(TYPE), typeResult(type) {}

  CodegenResult(CodegenResult &&other) = default;
  CodegenResult &operator=(CodegenResult &&other) = default;

  ~CodegenResult() noexcept;
};

class CodegenVisitor
    : public CallbackASTVisitor<CodegenResult, CodegenResult, CodegenResult> {
  shared_ptr<CodegenContext> ctx;
  CodegenResult result;

  void defaultVisit(const Expr *expr) override;
  void defaultVisit(const Stmt *expr) override;
  void defaultVisit(const Pattern *expr) override;

public:
  explicit CodegenVisitor(shared_ptr<CodegenContext> ctx);
  static seq::ir::SIRModulePtr apply(shared_ptr<Cache> cache, StmtPtr stmts);

  CodegenResult transform(const ExprPtr &expr) override;
  CodegenResult transform(const StmtPtr &stmt) override;
  CodegenResult transform(const PatternPtr &pat) override { assert(false); }

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
  seq::ir::OperandPtr toOperand(CodegenResult r);
  seq::ir::RvaluePtr toRvalue(CodegenResult r);

  std::unique_ptr<ir::SeriesFlow> newScope(const seq::SrcObject *s, std::string name);

  seq::ir::Flow *nearestLoop();

  template <typename Tn, typename... Ts>
  auto Nr(const seq::SrcObject *s, Ts &&... args) {
    auto t = new Tn(std::forward<Ts>(args)...);
    t->setSrcInfo(s->getSrcInfo());
    return t;
  }

  template <typename Tn, typename... Ts> auto Nr(const seq::SrcInfo s, Ts &&... args) {
    auto t = new Tn(std::forward<Ts>(args)...);
    t->setSrcInfo(s);
    return t;
  }

  template <typename T> auto wrap(T *obj) { return std::unique_ptr<T>(obj); }
};

} // namespace ast
} // namespace seq
