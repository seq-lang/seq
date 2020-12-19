#include "instr.h"

#include "util/iterators.h"

namespace seq {
namespace ir {

std::ostream &AssignInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("store({}{}{}, {})"), *lhs, field.empty() ? "" : ".", field,
             *rhs);
  return os;
}

types::Type *LoadInstr::getType() const {
  auto *ptrType = dynamic_cast<types::PointerType *>(ptr->getType());
  assert(ptrType);
  if (!field.empty()) {
    auto *memberedBase = dynamic_cast<types::MemberedType *>(ptrType->getBase());
    assert(memberedBase);
    return memberedBase->getMemberType(field);
  }
  return ptrType->getBase();
}

std::ostream &LoadInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("load({}{}{})"), *ptr, field.empty() ? "" : ".", field);
  return os;
}

types::Type *CallInstr::getType() const {
  auto *funcType = dynamic_cast<types::FuncType *>(func->getType());
  assert(funcType);
  return funcType->getReturnType();
}

std::ostream &CallInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("call({}, {})"), *func,
             fmt::join(util::dereference_adaptor(args.begin()),
                       util::dereference_adaptor(args.end()), ", "));
  return os;
}

std::ostream &StackAllocInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("stack_alloc({})"), *arrayType, *count);
  return os;
}

std::ostream &YieldInInstr::doFormat(std::ostream &os) const {
  return os << "yield_in()";
}

std::ostream &TernaryInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("ternary({}, {}, {})"), *cond, trueValue, *falseValue);
  return os;
}

std::ostream &BreakInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("break({})"), *getTarget());
  return os;
}

std::ostream &ContinueInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("continue({})"), *getTarget());
  return os;
}

std::ostream &ReturnInstr::doFormat(std::ostream &os) const {
  if (value) {
    fmt::print(os, FMT_STRING("return({})"), *value);
  } else {
    os << "return()";
  }
  return os;
}

std::ostream &YieldInstr::doFormat(std::ostream &os) const {
  if (value) {
    fmt::print(os, FMT_STRING("yield({})"), *value);
  } else {
    os << "yield()";
  }
  return os;
}

std::ostream &ThrowInstr::doFormat(std::ostream &os) const {
  if (value) {
    fmt::print(os, FMT_STRING("throw({})"), *value);
  } else {
    os << "throw()";
  }
  return os;
}

std::ostream &AssertInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("assert({}, \"{}\")"), *value, msg);
  return os;
}

std::ostream &FlowInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("inline_flow({}, {})"), *flow, *val);
  return os;
}

} // namespace ir
} // namespace seq
