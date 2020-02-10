#pragma once

#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/codegen/stmt.h"
#include "parser/ast/expr.h"
#include "parser/ast/visitor.h"
#include "parser/common.h"
#include "parser/context.h"

namespace seq {
namespace ast {

class CodegenExprVisitor : public ExprVisitor {
  Context &ctx;
  CodegenStmtVisitor &stmtVisitor;
  seq::Expr *result;
  std::vector<seq::Var *> *captures;
  friend class CodegenStmtVisitor;

public:
  CodegenExprVisitor(Context &ctx, CodegenStmtVisitor &stmtVisitor,
                     std::vector<seq::Var *> *captures = nullptr);
  seq::Expr *transform(const ExprPtr &e);
  seq::types::Type *transformType(const ExprPtr &expr);
  seq::For *parseComprehension(const Expr *expr,
                               const std::vector<GeneratorExpr::Body> &loops,
                               int &added);

  void visit(const EmptyExpr *) override;
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
};

} // namespace ast
} // namespace seq
