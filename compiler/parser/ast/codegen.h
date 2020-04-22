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
#include "parser/ast/ast.h"
#include "parser/ast/visitor.h"
#include "parser/ast/walk.h"
#include "parser/common.h"
#include "parser/context.h"

namespace seq {
namespace ast {

#ifdef HAHA

class CodegenVisitor
    : public ASTVisitor<seq::Expr *, seq::Stmt *, seq::Pattern *> {
  Context &ctx;
  seq::Expr *resultExpr;
  seq::Stmt *resultStmt;
  seq::Pattern *resultPattern;

  void defaultVisit(const Expr *expr) override;
  void defaultVisit(const Stmt *expr) override;
  void defaultVisit(const Pattern *expr) override;

  class CaptureExprVisitor : public WalkExprVisitor {
    Context &ctx;
    std::unordered_map<std::string, seq::Var *> captures;

  public:
    using WalkExprVisitor::visit;
    CaptureExprVisitor(Context &ctx);
    void visit(const IdExpr *) override;
  };

  seq::For *parseComprehension(const Expr *expr,
                               const std::vector<GeneratorExpr::Body> &loops,
                               int &added);

public:
  CodegenVisitor(Context &ctx);

  seq::Expr *transform(const Expr *expr) override;
  seq::Stmt *transform(const Stmt *stmt) override;
  seq::Pattern *transform(const Pattern *pat) override;
  seq::types::Type *transformType(const ExprPtr &expr);

public:
  void visit(const NoneExpr *) override;
  void visit(const BoolExpr *) override;
  void visit(const IntExpr *) override;
  void visit(const FloatExpr *) override;
  void visit(const StringExpr *) override;
  void visit(const FStringExpr *) override;
  void visit(const KmerExpr *) override;
  void visit(const SeqExpr *) override;
  void visit(const IdExpr *) override;
  void visit(const UnpackExpr *) override;
  void visit(const TupleExpr *) override;
  void visit(const ListExpr *) override;
  void visit(const SetExpr *) override;
  void visit(const DictExpr *) override;
  void visit(const GeneratorExpr *) override;
  void visit(const DictGeneratorExpr *) override;
  void visit(const IfExpr *) override;
  void visit(const UnaryExpr *) override;
  void visit(const BinaryExpr *) override;
  void visit(const PipeExpr *) override;
  void visit(const IndexExpr *) override;
  void visit(const CallExpr *) override;
  void visit(const DotExpr *) override;
  void visit(const SliceExpr *) override;
  void visit(const EllipsisExpr *) override;
  void visit(const TypeOfExpr *) override;
  void visit(const PtrExpr *) override;
  void visit(const LambdaExpr *) override;
  void visit(const YieldExpr *) override;

  void visit(const SuiteStmt *) override;
  void visit(const PassStmt *) override;
  void visit(const BreakStmt *) override;
  void visit(const ContinueStmt *) override;
  void visit(const ExprStmt *) override;
  void visit(const AssignStmt *) override;
  void visit(const DelStmt *) override;
  void visit(const PrintStmt *) override;
  void visit(const ReturnStmt *) override;
  void visit(const YieldStmt *) override;
  void visit(const AssertStmt *) override;
  void visit(const WhileStmt *) override;
  void visit(const ForStmt *) override;
  void visit(const IfStmt *) override;
  void visit(const MatchStmt *) override;
  void visit(const ExtendStmt *) override;
  void visit(const ImportStmt *) override;
  void visit(const ExternImportStmt *) override;
  void visit(const TryStmt *) override;
  void visit(const GlobalStmt *) override;
  void visit(const ThrowStmt *) override;
  void visit(const FunctionStmt *) override;
  void visit(const ClassStmt *) override;
  void visit(const AssignEqStmt *) override;
  void visit(const YieldFromStmt *) override;
  void visit(const WithStmt *) override;
  void visit(const PyDefStmt *) override;
  void visit(const DeclareStmt *) override;

  void visit(const StarPattern *) override;
  void visit(const IntPattern *) override;
  void visit(const BoolPattern *) override;
  void visit(const StrPattern *) override;
  void visit(const SeqPattern *) override;
  void visit(const RangePattern *) override;
  void visit(const TuplePattern *) override;
  void visit(const ListPattern *) override;
  void visit(const OrPattern *) override;
  void visit(const WildcardPattern *) override;
  void visit(const GuardedPattern *) override;
  void visit(const BoundPattern *) override;
};

#endif
} // namespace ast
} // namespace seq
