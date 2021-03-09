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

types::Type *Instr::doGetType() const { return getModule()->getVoidType(); }

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

const char ExtractInstr::NodeId = 0;

types::Type *ExtractInstr::doGetType() const {
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

const char CallInstr::NodeId = 0;

types::Type *CallInstr::doGetType() const {
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

const char TypePropertyInstr::NodeId = 0;

types::Type *TypePropertyInstr::doGetType() const {
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

const char ControlFlowInstr::NodeId = 0;

const char BreakInstr::NodeId = 0;

std::ostream &BreakInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("break()"));
  return os;
}

const char ContinueInstr::NodeId = 0;

std::ostream &ContinueInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("continue()"));
  return os;
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
    fmt::print(os, FMT_STRING("yield{}({})"), final ? "_final" : "", *value);
  } else {
    os << (final ? "yield_final()" : "yield()");
  }
  return os;
}

const char ThrowInstr::NodeId = 0;

std::vector<Value *> ThrowInstr::doGetUsedValues() const {
  if (value)
    return {value};
  return {};
}

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

} // namespace ir
} // namespace seq
