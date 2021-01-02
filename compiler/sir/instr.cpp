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

Value *AssignInstr::doClone() const {
  return getModule()->Nrs<AssignInstr>(getSrcInfo(), lhs, rhs->clone(), getName());
}

const char ExtractInstr::NodeId = 0;

const types::Type *ExtractInstr::getType() const {
  auto *memberedType = val->getType()->as<types::MemberedType>();
  assert(memberedType);
  return memberedType->getMemberType(field);
}

Value *ExtractInstr::doClone() const {
  return getModule()->Nrs<ExtractInstr>(getSrcInfo(), val->clone(), field, getName());
}

std::ostream &ExtractInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("extract({}, \"{}\")"), *val, field);
  return os;
}

const char InsertInstr::NodeId = 0;

std::ostream &InsertInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("insert({}, \"{}\", {})"), *lhs, field, *rhs);
  return os;
}

Value *InsertInstr::doClone() const {
  return getModule()->Nrs<InsertInstr>(getSrcInfo(), lhs->clone(), field, rhs->clone(),
                                       getName());
}

const char CallInstr::NodeId = 0;

const types::Type *CallInstr::getType() const {
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

Value *CallInstr::doClone() const {
  std::vector<ValuePtr> clonedArgs;
  for (const auto *arg : *this)
    clonedArgs.push_back(arg->clone());
  return getModule()->Nrs<CallInstr>(getSrcInfo(), func->clone(), std::move(clonedArgs),
                                     getName());
}

const char StackAllocInstr::NodeId = 0;

std::ostream &StackAllocInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("stack_alloc({}, {})"), arrayType->referenceString(),
             *count);
  return os;
}

Value *StackAllocInstr::doClone() const {
  return getModule()->Nrs<StackAllocInstr>(getSrcInfo(), arrayType, count->clone(),
                                           getName());
}

const char YieldInInstr::NodeId = 0;

std::ostream &YieldInInstr::doFormat(std::ostream &os) const {
  return os << "yield_in()";
}

Value *YieldInInstr::doClone() const {
  return getModule()->Nrs<YieldInInstr>(getSrcInfo(), type, suspend, getName());
}

const char TernaryInstr::NodeId = 0;

std::ostream &TernaryInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("ternary({}, {}, {})"), *cond, trueValue, *falseValue);
  return os;
}

Value *TernaryInstr::doClone() const {
  return getModule()->Nrs<TernaryInstr>(getSrcInfo(), cond->clone(), trueValue->clone(),
                                        falseValue->clone(), getName());
}

const char ControlFlowInstr::NodeId = 0;

const char BreakInstr::NodeId = 0;

std::ostream &BreakInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("break({})"), target->referenceString());
  return os;
}

Value *BreakInstr::doClone() const {
  return getModule()->Nrs<BreakInstr>(getSrcInfo(), target, getName());
}

const char ContinueInstr::NodeId = 0;

std::ostream &ContinueInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("continue({})"), target->referenceString());
  return os;
}

Value *ContinueInstr::doClone() const {
  return getModule()->Nrs<ContinueInstr>(getSrcInfo(), target, getName());
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

Value *ReturnInstr::doClone() const {
  return getModule()->Nrs<ReturnInstr>(getSrcInfo(), value ? value->clone() : nullptr,
                                       getName());
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

Value *YieldInstr::doClone() const {
  return getModule()->Nrs<YieldInstr>(getSrcInfo(), value ? value->clone() : nullptr,
                                      getName());
}

const char ThrowInstr::NodeId = 0;

std::ostream &ThrowInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("throw({})"), *value);
  return os;
}

Value *ThrowInstr::doClone() const {
  return getModule()->Nrs<ThrowInstr>(getSrcInfo(), value->clone(), getName());
}

const char FlowInstr::NodeId = 0;

std::ostream &FlowInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("inline_flow({}, {})"), *flow, *val);
  return os;
}

Value *FlowInstr::doClone() const {
  return getModule()->Nrs<FlowInstr>(getSrcInfo(), flow->clone(), val->clone(),
                                     getName());
}

} // namespace ir
} // namespace seq
