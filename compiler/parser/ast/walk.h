/// walk.h: toy AST walker
/// Should delete it later...
#pragma once

#include "parser/ast/ast/ast.h"

namespace seq {
namespace ast {

struct WalkVisitor : public ASTVisitor {
  virtual void visit(const StarExpr *) override;
  virtual void visit(const TupleExpr *) override;
  virtual void visit(const ListExpr *) override;
  virtual void visit(const SetExpr *) override;
  virtual void visit(const DictExpr *) override;
  virtual void visit(const GeneratorExpr *) override;
  virtual void visit(const DictGeneratorExpr *) override;
  virtual void visit(const IfExpr *) override;
  virtual void visit(const UnaryExpr *) override;
  virtual void visit(const BinaryExpr *) override;
  virtual void visit(const PipeExpr *) override;
  virtual void visit(const IndexExpr *) override;
  virtual void visit(const CallExpr *) override;
  virtual void visit(const DotExpr *) override;
  virtual void visit(const SliceExpr *) override;
  virtual void visit(const TypeOfExpr *) override;
  virtual void visit(const PtrExpr *) override;
};

} // namespace ast
} // namespace seq
