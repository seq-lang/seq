#include "instr.h"

#include "module.h"
#include "util/iterators.h"

namespace seq {
namespace ir {

const char Instr::NodeId = 0;

const char AssignInstr::NodeId = 0;

std::ostream &AssignInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("store({}, {})"), *lhs, *rhs);
  return os;
}

const char LoadInstr::NodeId = 0;

types::Type *LoadInstr::getType() const {
  auto *ptrType = ptr->getType()->as<types::PointerType>();
  assert(ptrType);
  return ptrType->getBase();
}

const char GetFieldPtrInstr::NodeId = 0;

types::Type *GetFieldPtrInstr::getType() const {
  auto *ptrType = ptr->getType()->as<types::PointerType>();
  assert(ptrType);
  auto *base = ptrType->getBase()->as<types::MemberedType>();
  assert(base);
  auto *member = base->getMemberType(field);
  assert(member);
  return getModule()->getPointerType(member);
}

std::ostream &GetFieldPtrInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("gfp({}, {})"), *ptr, field);
  return os;
}

std::ostream &LoadInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("load({})"), *ptr);
  return os;
}

const char CallInstr::NodeId = 0;

types::Type *CallInstr::getType() const {
  auto *funcType = func->getType()->as<types::FuncType>();
  assert(funcType);
  return funcType->getReturnType();
}

std::ostream &CallInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("call({}, {})"), *func,
             fmt::join(util::dereference_adaptor(args.begin()),
                       util::dereference_adaptor(args.end()), ", "));
  return os;
}

const char StackAllocInstr::NodeId = 0;

std::ostream &StackAllocInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("stack_alloc({})"), *arrayType, *count);
  return os;
}

const char YieldInInstr::NodeId = 0;

std::ostream &YieldInInstr::doFormat(std::ostream &os) const {
  return os << "yield_in()";
}

const char TernaryInstr::NodeId = 0;

std::ostream &TernaryInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("ternary({}, {}, {})"), *cond, trueValue, *falseValue);
  return os;
}

const char ControlFlowInstr::NodeId = 0;

const char BreakInstr::NodeId = 0;

std::ostream &BreakInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("break({})"), *getTarget());
  return os;
}

const char ContinueInstr::NodeId = 0;

std::ostream &ContinueInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("continue({})"), *getTarget());
  return os;
}

const char ReturnInstr::NodeId = 0;

std::ostream &ReturnInstr::doFormat(std::ostream &os) const {
  if (value) {
    fmt::print(os, FMT_STRING("return({})"), *value);
  } else {
    os << "return()";
  }
  return os;
}

const char YieldInstr::NodeId = 0;

std::ostream &YieldInstr::doFormat(std::ostream &os) const {
  if (value) {
    fmt::print(os, FMT_STRING("yield({})"), *value);
  } else {
    os << "yield()";
  }
  return os;
}

const char ThrowInstr::NodeId = 0;

std::ostream &ThrowInstr::doFormat(std::ostream &os) const {
  if (value) {
    fmt::print(os, FMT_STRING("throw({})"), *value);
  } else {
    os << "throw()";
  }
  return os;
}

const char AssertInstr::NodeId = 0;

std::ostream &AssertInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("assert({}, \"{}\")"), *value, msg);
  return os;
}

const char FlowInstr::NodeId = 0;

std::ostream &FlowInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("inline_flow({}, {})"), *flow, *val);
  return os;
}

} // namespace ir
} // namespace seq
