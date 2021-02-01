#include "instr.h"

#include "module.h"
#include "util/iterators.h"

namespace {

int findAndReplace(int id, seq::ir::Value *newVal,
                   std::vector<seq::ir::Value *> &values) {
  auto replacements = 0;
  for (auto &value : values) {
    if (value->getId() == id) {
      value = newVal;
      ++replacements;
    }
  }
  return replacements;
}

} // namespace

namespace seq {
namespace ir {

const char Instr::NodeId = 0;

const types::Type *Instr::doGetType() const { return getModule()->getVoidType(); }

const char AssignInstr::NodeId = 0;

int AssignInstr::doReplaceUsedValue(int id, Value *newValue) {
  if (rhs->getId() == id) {
    rhs = newValue;
    return 1;
  }
  return 0;
}

int AssignInstr::doReplaceUsedVariable(int id, Var *newVar) {
  if (lhs->getId() == id) {
    lhs = newVar;
    return 1;
  }
  return 0;
}

std::ostream &AssignInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("store({}, {})"), *lhs, *rhs);
  return os;
}

Value *AssignInstr::doClone() const {
  return getModule()->N<AssignInstr>(getSrcInfo(), lhs, rhs->clone(), getName());
}

const char ExtractInstr::NodeId = 0;

const types::Type *ExtractInstr::doGetType() const {
  auto *memberedType = val->getType()->as<types::MemberedType>();
  assert(memberedType);
  return memberedType->getMemberType(field);
}

int ExtractInstr::doReplaceUsedValue(int id, Value *newValue) {
  if (val->getId() == id) {
    val = newValue;
    return 1;
  }
  return 0;
}

Value *ExtractInstr::doClone() const {
  return getModule()->N<ExtractInstr>(getSrcInfo(), val->clone(), field, getName());
}

std::ostream &ExtractInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("extract({}, \"{}\")"), *val, field);
  return os;
}

const char InsertInstr::NodeId = 0;

int InsertInstr::doReplaceUsedValue(int id, Value *newValue) {
  auto replacements = 0;
  if (lhs->getId() == id) {
    lhs = newValue;
    ++replacements;
  }
  if (rhs->getId() == id) {
    rhs = newValue;
    ++replacements;
  }
  return replacements;
}

std::ostream &InsertInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("insert({}, \"{}\", {})"), *lhs, field, *rhs);
  return os;
}

Value *InsertInstr::doClone() const {
  return getModule()->N<InsertInstr>(getSrcInfo(), lhs->clone(), field, rhs->clone(),
                                     getName());
}

const char CallInstr::NodeId = 0;

const types::Type *CallInstr::doGetType() const {
  auto *funcType = func->getType()->as<types::FuncType>();
  assert(funcType);
  return funcType->getReturnType();
}

std::vector<Value *> CallInstr::doGetUsedValues() const {
  std::vector<Value *> ret(args.begin(), args.end());
  ret.push_back(func);
  return ret;
}

int CallInstr::doReplaceUsedValue(int id, Value *newValue) {
  auto replacements = 0;
  if (func->getId() == id) {
    func = newValue;
    ++replacements;
  }
  replacements += findAndReplace(id, newValue, args);
  return replacements;
}

std::ostream &CallInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("call({}, {})"), *func,
             fmt::join(util::dereference_adaptor(args.begin()),
                       util::dereference_adaptor(args.end()), ", "));
  return os;
}

Value *CallInstr::doClone() const {
  std::vector<Value *> clonedArgs;
  for (const auto *arg : *this)
    clonedArgs.push_back(arg->clone());
  return getModule()->N<CallInstr>(getSrcInfo(), func->clone(), std::move(clonedArgs),
                                   getName());
}

const char StackAllocInstr::NodeId = 0;

std::ostream &StackAllocInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("stack_alloc({}, {})"), arrayType->referenceString(),
             count);
  return os;
}

int StackAllocInstr::doReplaceUsedType(const std::string &name, types::Type *newType) {
  if (arrayType->getName() == name) {
    arrayType = newType;
    return 1;
  }
  return 0;
}

Value *StackAllocInstr::doClone() const {
  return getModule()->N<StackAllocInstr>(getSrcInfo(), arrayType, count, getName());
}

const char TypePropertyInstr::NodeId = 0;

const types::Type *TypePropertyInstr::doGetType() const {
  switch (property) {
  case Property::IS_ATOMIC:
    return getModule()->getBoolType();
  case Property::SIZEOF:
    return getModule()->getIntType();
  default:
    return getModule()->getVoidType();
  }
}

std::ostream &TypePropertyInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("type_property({}, {})"),
             property == Property::IS_ATOMIC ? "atomic" : "sizeof",
             inspectType->referenceString());
  return os;
}

int TypePropertyInstr::doReplaceUsedType(const std::string &name,
                                         types::Type *newType) {
  if (inspectType->getName() == name) {
    inspectType = newType;
    return 1;
  }
  return 0;
}

Value *TypePropertyInstr::doClone() const {
  return getModule()->N<TypePropertyInstr>(getSrcInfo(), inspectType, property,
                                           getName());
}

const char YieldInInstr::NodeId = 0;

int YieldInInstr::doReplaceUsedType(const std::string &name, types::Type *newType) {
  if (type->getName() == name) {
    type = newType;
    return 1;
  }
  return 0;
}

std::ostream &YieldInInstr::doFormat(std::ostream &os) const {
  return os << "yield_in()";
}

Value *YieldInInstr::doClone() const {
  return getModule()->N<YieldInInstr>(getSrcInfo(), type, suspend, getName());
}

const char TernaryInstr::NodeId = 0;

int TernaryInstr::doReplaceUsedValue(int id, Value *newValue) {
  auto replacements = 0;
  if (cond->getId() == id) {
    cond = newValue;
    ++replacements;
  }
  if (trueValue->getId() == id) {
    trueValue = newValue;
    ++replacements;
  }
  if (falseValue->getId() == id) {
    falseValue = newValue;
    ++replacements;
  }
  return replacements;
}

std::ostream &TernaryInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("ternary({}, {}, {})"), *cond, *trueValue, *falseValue);
  return os;
}

Value *TernaryInstr::doClone() const {
  return getModule()->N<TernaryInstr>(getSrcInfo(), cond->clone(), trueValue->clone(),
                                      falseValue->clone(), getName());
}

const char ControlFlowInstr::NodeId = 0;

int ControlFlowInstr::doReplaceUsedValue(int id, Value *newValue) {
  if (target->getId() == id) {
    auto *flow = cast<Flow>(newValue);
    assert(flow);
    target = flow;
    return 1;
  }
  return 0;
}

const char BreakInstr::NodeId = 0;

std::ostream &BreakInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("break({})"), target->referenceString());
  return os;
}

Value *BreakInstr::doClone() const {
  return getModule()->N<BreakInstr>(getSrcInfo(), const_cast<Flow *>(getTarget()),
                                    getName());
}

const char ContinueInstr::NodeId = 0;

std::ostream &ContinueInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("continue({})"), target->referenceString());
  return os;
}

Value *ContinueInstr::doClone() const {
  return getModule()->N<ContinueInstr>(getSrcInfo(), const_cast<Flow *>(getTarget()),
                                       getName());
}

const char ReturnInstr::NodeId = 0;

std::vector<Value *> ReturnInstr::doGetUsedValues() const {
  if (value)
    return {value};
  return {};
}

int ReturnInstr::doReplaceUsedValue(int id, Value *newValue) {
  auto replacements = 0;
  if (value && value->getId() == id) {
    setValue(newValue);
    ++replacements;
  }
  replacements += ControlFlowInstr::replaceUsedValue(id, newValue);
  return replacements;
}

std::ostream &ReturnInstr::doFormat(std::ostream &os) const {
  if (value) {
    fmt::print(os, FMT_STRING("return({})"), *value);
  } else {
    os << "return()";
  }
  return os;
}

Value *ReturnInstr::doClone() const {
  return getModule()->N<ReturnInstr>(getSrcInfo(), value ? value->clone() : nullptr,
                                     getName());
}

const char YieldInstr::NodeId = 0;

std::vector<Value *> YieldInstr::doGetUsedValues() const {
  if (value)
    return {value};
  return {};
}

int YieldInstr::doReplaceUsedValue(int id, Value *newValue) {
  if (value && value->getId() == id) {
    setValue(newValue);
    return 1;
  }
  return 0;
}

std::ostream &YieldInstr::doFormat(std::ostream &os) const {
  if (value) {
    fmt::print(os, FMT_STRING("yield({})"), *value);
  } else {
    os << "yield()";
  }
  return os;
}

Value *YieldInstr::doClone() const {
  return getModule()->N<YieldInstr>(getSrcInfo(), value ? value->clone() : nullptr,
                                    getName());
}

const char ThrowInstr::NodeId = 0;

int ThrowInstr::doReplaceUsedValue(int id, Value *newValue) {
  if (value && value->getId() == id) {
    setValue(newValue);
    return 1;
  }
  return 0;
}

std::ostream &ThrowInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("throw({})"), *value);
  return os;
}

Value *ThrowInstr::doClone() const {
  return getModule()->N<ThrowInstr>(getSrcInfo(), value->clone(), getName());
}

const char FlowInstr::NodeId = 0;

int FlowInstr::doReplaceUsedValue(int id, Value *newValue) {
  auto replacements = 0;
  if (flow->getId() == id) {
    auto *f = cast<Flow>(newValue);
    assert(f);
    setFlow(f);
    ++replacements;
  }
  if (val->getId() == id) {
    setValue(newValue);
    ++replacements;
  }
  return replacements;
}

std::ostream &FlowInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("inline_flow({}, {})"), *flow, *val);
  return os;
}

Value *FlowInstr::doClone() const {
  return getModule()->N<FlowInstr>(getSrcInfo(), cast<Flow>(flow->clone()),
                                   val->clone(), getName());
}

} // namespace ir
} // namespace seq
