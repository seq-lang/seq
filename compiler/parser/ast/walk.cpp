#include "parser/ast/walk.h"

#define WALK(x)                                                                \
  if (x)                                                                       \
  x->accept(*this)

namespace seq {
namespace ast {

void WalkExprVisitor::visit(const UnpackExpr *e) { WALK(e->what); }

void WalkExprVisitor::visit(const TupleExpr *e) {
  for (auto &i : e->items)
    WALK(i);
}

void WalkExprVisitor::visit(const ListExpr *e) {
  for (auto &i : e->items)
    WALK(i);
}

void WalkExprVisitor::visit(const SetExpr *e) {
  for (auto &i : e->items)
    WALK(i);
}

void WalkExprVisitor::visit(const DictExpr *e) {
  for (auto &i : e->items) {
    WALK(i.key);
    WALK(i.value);
  }
}

void WalkExprVisitor::visit(const GeneratorExpr *e) {
  WALK(e->expr);
  for (auto &i : e->loops) {
    WALK(i.gen);
    for (auto &c : i.conds)
      WALK(c);
  }
}

void WalkExprVisitor::visit(const DictGeneratorExpr *e) {
  WALK(e->key);
  WALK(e->expr);
  for (auto &i : e->loops) {
    WALK(i.gen);
    for (auto &c : i.conds)
      WALK(c);
  }
}

void WalkExprVisitor::visit(const IfExpr *e) {
  WALK(e->cond);
  WALK(e->eif);
  WALK(e->eelse);
}

void WalkExprVisitor::visit(const UnaryExpr *e) { WALK(e->expr); }

void WalkExprVisitor::visit(const BinaryExpr *e) {
  WALK(e->lexpr);
  WALK(e->rexpr);
}

void WalkExprVisitor::visit(const PipeExpr *e) {
  for (auto &i : e->items)
    WALK(i.expr);
}

void WalkExprVisitor::visit(const IndexExpr *e) {
  WALK(e->expr);
  WALK(e->index);
}

void WalkExprVisitor::visit(const CallExpr *e) {
  WALK(e->expr);
  for (auto &i : e->args)
    WALK(i.value);
}

void WalkExprVisitor::visit(const DotExpr *e) { WALK(e->expr); }

void WalkExprVisitor::visit(const SliceExpr *e) {
  WALK(e->st);
  WALK(e->ed);
  WALK(e->step);
}

void WalkExprVisitor::visit(const TypeOfExpr *e) { WALK(e->expr); }

void WalkExprVisitor::visit(const PtrExpr *e) { WALK(e->expr); }

} // namespace ast
} // namespace seq
