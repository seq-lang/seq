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
#include "parser/ast/ast/ast.h"
#include "parser/ast/cache.h"
#include "parser/ast/codegen/codegen_ctx.h"
#include "parser/common.h"

namespace seq {
namespace ast {

class CodegenVisitor
    : public CallbackASTVisitor<seq::Expr *, seq::Stmt *, seq::Pattern *> {
  shared_ptr<CodegenContext> ctx;
  seq::Expr *resultExpr;
  seq::Stmt *resultStmt;
  seq::Pattern *resultPattern;

  void defaultVisit(const Expr *expr) override;
  void defaultVisit(const Stmt *expr) override;
  void defaultVisit(const Pattern *expr) override;

  seq::types::Type *realizeType(types::ClassTypePtr t);
  seq::BaseFunc *realizeFunc(const string &name);
  shared_ptr<CodegenItem> processIdentifier(shared_ptr<CodegenContext> tctx,
                                            const string &id);

public:
  CodegenVisitor(shared_ptr<CodegenContext> ctx);
  static seq::SeqModule *apply(shared_ptr<Cache> cache, StmtPtr stmts);

  seq::Expr *transform(const ExprPtr &expr) override;
  seq::Stmt *transform(const StmtPtr &stmt) override;
  seq::Pattern *transform(const PatternPtr &pat) override;
  seq::Stmt *transform(const StmtPtr &stmt, bool addToBlock);

  void visitMethods(const string &name);

public:
  void visit(const BoolExpr *) override;
  void visit(const IntExpr *) override;
  void visit(const FloatExpr *) override;
  void visit(const StringExpr *) override;
  void visit(const IdExpr *) override;
  void visit(const IfExpr *) override;
  void visit(const BinaryExpr *) override;
  void visit(const PipeExpr *) override;
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
  void visit(const MatchStmt *) override;
  void visit(const UpdateStmt *) override;
  void visit(const TryStmt *) override;
  void visit(const ThrowStmt *) override;
  void visit(const FunctionStmt *) override;
  void visit(const ClassStmt *stmt) override;

  void visit(const StarPattern *) override;
  void visit(const IntPattern *) override;
  void visit(const BoolPattern *) override;
  void visit(const StrPattern *) override;
  void visit(const RangePattern *) override;
  void visit(const TuplePattern *) override;
  void visit(const ListPattern *) override;
  void visit(const OrPattern *) override;
  void visit(const WildcardPattern *) override;
  void visit(const GuardedPattern *) override;
};

} // namespace ast
} // namespace seq
