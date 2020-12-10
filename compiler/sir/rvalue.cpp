#include "rvalue.h"

#include <utility>

#include "operand.h"
#include "var.h"

#include "util/visitor.h"

namespace seq {
namespace ir {

void Rvalue::accept(util::SIRVisitor &v) { v.visit(this); }

MemberRvalue::MemberRvalue(OperandPtr var, std::string field)
    : var(std::move(var)), field(std::move(field)) {
  this->var->parent = this;
}

void MemberRvalue::accept(util::SIRVisitor &v) { v.visit(this); }

types::Type *MemberRvalue::getType() const {
  return dynamic_cast<types::MemberedType *>(var->getType())->getMemberType(field);
}

std::ostream &MemberRvalue::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("({}).{}"), *var, field);
  return os;
}

CallRvalue::CallRvalue(OperandPtr func, std::vector<OperandPtr> args)
    : func(std::move(func)), args(std::move(args)) {
  for (auto &a : this->args) {
    a->parent = this;
  }
}

void CallRvalue::accept(util::SIRVisitor &v) { v.visit(this); }

types::Type *CallRvalue::getType() const {
  return dynamic_cast<types::FuncType *>(func->getType())->rType;
}

std::ostream &CallRvalue::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}("), *func);
  for (auto it = args.begin(); it != args.end(); it++) {
    fmt::print(os, FMT_STRING("{}"), *(*it));
    if (it + 1 != args.end())
      os << ", ";
  }
  os << ')';
  return os;
}

OperandRvalue::OperandRvalue(OperandPtr operand) : operand(std::move(operand)) {}

void OperandRvalue::accept(util::SIRVisitor &v) { v.visit(this); }

types::Type *OperandRvalue::getType() const { return operand->getType(); }

std::ostream &OperandRvalue::doFormat(std::ostream &os) const { return os << *operand; }

StackAllocRvalue::StackAllocRvalue(types::ArrayType *arrayType, OperandPtr count)
    : arrayType(arrayType), count(std::move(count)) {
  this->count->parent = this;
}

void StackAllocRvalue::accept(util::SIRVisitor &v) { v.visit(this); }

std::ostream &StackAllocRvalue::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("new({}, {})"), arrayType->referenceString(), count);
  return os;
}

void YieldInRvalue::accept(util::SIRVisitor &v) { v.visit(this); }

std::ostream &YieldInRvalue::doFormat(std::ostream &os) const {
  return os << "(yield)";
}

} // namespace ir
} // namespace seq
