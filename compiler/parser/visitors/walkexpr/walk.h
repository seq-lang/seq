/// walk.h: toy AST walker
/// Should delete it later...
#pragma once

#include "parser/ast.h"
#include "parser/visitors/visitor.h"

namespace seq {
namespace ast {

struct WalkVisitor : public ASTVisitor {
  virtual void visit(StarExpr *) override;
  virtual void visit(TupleExpr *) override;
  virtual void visit(ListExpr *) override;
  virtual void visit(SetExpr *) override;
  virtual void visit(DictExpr *) override;
  virtual void visit(GeneratorExpr *) override;
  virtual void visit(DictGeneratorExpr *) override;
  virtual void visit(IfExpr *) override;
  virtual void visit(UnaryExpr *) override;
  virtual void visit(BinaryExpr *) override;
  virtual void visit(PipeExpr *) override;
  virtual void visit(IndexExpr *) override;
  virtual void visit(CallExpr *) override;
  virtual void visit(DotExpr *) override;
  virtual void visit(SliceExpr *) override;
  virtual void visit(TypeOfExpr *) override;
  virtual void visit(PtrExpr *) override;
};

} // namespace ast
} // namespace seq
