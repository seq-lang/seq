#include "parser/visitors/walkexpr/walk.h"

#define WALK(x)                                                                        \
  if (x)                                                                               \
  x->accept(*this)

namespace seq {
namespace ast {

void WalkVisitor::visit(StarExpr *e) { WALK(e->what); }

void WalkVisitor::visit(TupleExpr *e) {
  for (auto &i : e->items)
    WALK(i);
}

void WalkVisitor::visit(ListExpr *e) {
  for (auto &i : e->items)
    WALK(i);
}

void WalkVisitor::visit(SetExpr *e) {
  for (auto &i : e->items)
    WALK(i);
}

void WalkVisitor::visit(DictExpr *e) {
  for (auto &i : e->items) {
    WALK(i.key);
    WALK(i.value);
  }
}

void WalkVisitor::visit(GeneratorExpr *e) {
  WALK(e->expr);
  for (auto &i : e->loops) {
    WALK(i.gen);
    for (auto &c : i.conds)
      WALK(c);
  }
}

void WalkVisitor::visit(DictGeneratorExpr *e) {
  WALK(e->key);
  WALK(e->expr);
  for (auto &i : e->loops) {
    WALK(i.gen);
    for (auto &c : i.conds)
      WALK(c);
  }
}

void WalkVisitor::visit(IfExpr *e) {
  WALK(e->cond);
  WALK(e->ifexpr);
  WALK(e->elsexpr);
}

void WalkVisitor::visit(UnaryExpr *e) { WALK(e->expr); }

void WalkVisitor::visit(BinaryExpr *e) {
  WALK(e->lexpr);
  WALK(e->rexpr);
}

void WalkVisitor::visit(PipeExpr *e) {
  for (auto &i : e->items)
    WALK(i.expr);
}

void WalkVisitor::visit(IndexExpr *e) {
  WALK(e->expr);
  WALK(e->index);
}

void WalkVisitor::visit(CallExpr *e) {
  WALK(e->expr);
  for (auto &i : e->args)
    WALK(i.value);
}

void WalkVisitor::visit(DotExpr *e) { WALK(e->expr); }

void WalkVisitor::visit(SliceExpr *e) {
  WALK(e->start);
  WALK(e->stop);
  WALK(e->step);
}

void WalkVisitor::visit(TypeOfExpr *e) { WALK(e->expr); }

void WalkVisitor::visit(PtrExpr *e) { WALK(e->expr); }

} // namespace ast
} // namespace seq
