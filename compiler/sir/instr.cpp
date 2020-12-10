#include "instr.h"

#include "util/fmt/format.h"

#include "flow.h"
#include "lvalue.h"
#include "rvalue.h"

#include "util/visitor.h"

namespace seq {
namespace ir {

std::string Instr::referenceString() const {
  return fmt::format(FMT_STRING("{}.{}"), parent ? parent->referenceString() : "",
                     name);
}

void Instr::accept(util::SIRVisitor &v) { v.visit(this); }

AssignInstr::AssignInstr(LvaluePtr left, RvaluePtr right)
    : left(std::move(left)), right(std::move(right)) {
  this->left->parent = this;
  this->right->parent = this;
}

void AssignInstr::accept(util::SIRVisitor &v) { v.visit(this); }

std::ostream &AssignInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{} = {};"), *left, *right);
  return os;
}

RvalueInstr::RvalueInstr(RvaluePtr rvalue) : rvalue(std::move(rvalue)) {
  this->rvalue->parent = this;
}

void RvalueInstr::accept(util::SIRVisitor &v) { v.visit(this); }

std::ostream &RvalueInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{};"), *rvalue);
  return os;
}

void BreakInstr::accept(util::SIRVisitor &v) { v.visit(this); }

std::ostream &BreakInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("break {};"), target->referenceString());
  return os;
}

void ContinueInstr::accept(util::SIRVisitor &v) { v.visit(this); }

std::ostream &ContinueInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("continue {};"), target->referenceString());
  return os;
}

ReturnInstr::ReturnInstr(RvaluePtr value) : value(std::move(value)) {
  if (value)
    this->value->parent = this;
}

void ReturnInstr::accept(util::SIRVisitor &v) { v.visit(this); }

std::ostream &ReturnInstr::doFormat(std::ostream &os) const {
  if (value) {
    fmt::print(os, FMT_STRING("return {};"), *value);
  } else {
    os << "return;";
  }
  return os;
}

YieldInstr::YieldInstr(RvaluePtr value) : value(std::move(value)) {
  if (value)
    this->value->parent = this;
}

void YieldInstr::accept(util::SIRVisitor &v) { v.visit(this); }

std::ostream &YieldInstr::doFormat(std::ostream &os) const {
  if (value) {
    fmt::print(os, FMT_STRING("yield {};"), *value);
  } else {
    os << "yield;";
  }
  return os;
}

ThrowInstr::ThrowInstr(RvaluePtr value) : value(std::move(value)) {
  if (value)
    this->value->parent = this;
}

void ThrowInstr::accept(util::SIRVisitor &v) { v.visit(this); }

std::ostream &ThrowInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("throw {};"), *value);
  return os;
}

AssertInstr::AssertInstr(RvaluePtr value, std::string msg)
    : value(std::move(value)), msg(std::move(msg)) {
  if (value)
    this->value->parent = this;
}

void AssertInstr::accept(util::SIRVisitor &v) { v.visit(this); }

std::ostream &AssertInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("assert {}, \"{}\";"), *value, msg);
  return os;
}

} // namespace ir
} // namespace seq
