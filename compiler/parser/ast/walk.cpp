#include "parser/ast/walk.h"

#define WALK(x)                                                                        \
  if (x)                                                                               \
  x->accept(*this)

namespace seq {
namespace ast {

void WalkVisitor::visit(const UnpackExpr *e) { WALK(e->what); }

void WalkVisitor::visit(const TupleExpr *e) {
  for (auto &i : e->items)
    WALK(i);
}

void WalkVisitor::visit(const ListExpr *e) {
  for (auto &i : e->items)
    WALK(i);
}

void WalkVisitor::visit(const SetExpr *e) {
  for (auto &i : e->items)
    WALK(i);
}

void WalkVisitor::visit(const DictExpr *e) {
  for (auto &i : e->items) {
    WALK(i.key);
    WALK(i.value);
  }
}

void WalkVisitor::visit(const GeneratorExpr *e) {
  WALK(e->expr);
  for (auto &i : e->loops) {
    WALK(i.gen);
    for (auto &c : i.conds)
      WALK(c);
  }
}

void WalkVisitor::visit(const DictGeneratorExpr *e) {
  WALK(e->key);
  WALK(e->expr);
  for (auto &i : e->loops) {
    WALK(i.gen);
    for (auto &c : i.conds)
      WALK(c);
  }
}

void WalkVisitor::visit(const IfExpr *e) {
  WALK(e->cond);
  WALK(e->eif);
  WALK(e->eelse);
}

void WalkVisitor::visit(const UnaryExpr *e) { WALK(e->expr); }

void WalkVisitor::visit(const BinaryExpr *e) {
  WALK(e->lexpr);
  WALK(e->rexpr);
}

void WalkVisitor::visit(const PipeExpr *e) {
  for (auto &i : e->items)
    WALK(i.expr);
}

void WalkVisitor::visit(const IndexExpr *e) {
  WALK(e->expr);
  WALK(e->index);
}

void WalkVisitor::visit(const CallExpr *e) {
  WALK(e->expr);
  for (auto &i : e->args)
    WALK(i.value);
}

void WalkVisitor::visit(const DotExpr *e) { WALK(e->expr); }

void WalkVisitor::visit(const SliceExpr *e) {
  WALK(e->st);
  WALK(e->ed);
  WALK(e->step);
}

void WalkVisitor::visit(const TypeOfExpr *e) { WALK(e->expr); }

void WalkVisitor::visit(const PtrExpr *e) { WALK(e->expr); }

} // namespace ast
} // namespace seq
