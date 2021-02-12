#pragma once

#include <memory>
#include <string>

#include "flow.h"
#include "types/types.h"
#include "util/iterators.h"
#include "value.h"
#include "var.h"

namespace seq {
namespace ir {

/// SIR object representing an "instruction," or discrete operation in the context of a
/// block.
class Instr : public AcceptorExtend<Instr, Value> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

private:
  const types::Type *doGetType() const override;
};

/// Instr representing setting a memory location.
class AssignInstr : public AcceptorExtend<AssignInstr, Instr> {
private:
  /// the left-hand side
  Var *lhs;
  /// the right-hand side
  Value *rhs;

public:
  static const char NodeId;

  /// Constructs an assign instruction.
  /// @param lhs the left-hand side
  /// @param rhs the right-hand side
  /// @param field the field being set, may be empty
  /// @param name the instruction's name
  AssignInstr(Var *lhs, Value *rhs, std::string name = "")
      : AcceptorExtend(std::move(name)), lhs(lhs), rhs(rhs) {}

  /// @return the left-hand side
  Var *getLhs() { return lhs; }
  /// @return the left-hand side
  const Var *getLhs() const { return lhs; }
  /// Sets the left-hand side
  /// @param l the new value
  void setLhs(Var *v) { lhs = v; }

  /// @return the right-hand side
  Value *getRhs() { return rhs; }
  /// @return the right-hand side
  const Value *getRhs() const { return rhs; }
  /// Sets the right-hand side
  /// @param l the new value
  void setRhs(Value *v) { rhs = v; }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  std::vector<Value *> doGetUsedValues() const override { return {rhs}; }
  int doReplaceUsedValue(int id, Value *newValue) override;

  std::vector<Var *> doGetUsedVariables() const override { return {lhs}; }
  int doReplaceUsedVariable(int id, Var *newVar) override;

  Value *doClone() const override;
};

/// Instr representing loading the field of a value.
class ExtractInstr : public AcceptorExtend<ExtractInstr, Instr> {
private:
  /// the value being manipulated
  Value *val;
  /// the field
  std::string field;

public:
  static const char NodeId;

  /// Constructs a load instruction.
  /// @param val the value being manipulated
  /// @param field the field
  /// @param name the instruction's name
  explicit ExtractInstr(Value *val, std::string field, std::string name = "")
      : AcceptorExtend(std::move(name)), val(val), field(std::move(field)) {}

  /// @return the location
  Value *getVal() { return val; }
  /// @return the location
  const Value *getVal() const { return val; }
  /// Sets the location.
  /// @param p the new value
  void setVal(Value *p) { val = p; }

  /// @return the field
  const std::string &getField() const { return field; }
  /// Sets the field.
  /// @param f the new field
  void setField(std::string f) { field = std::move(f); }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  const types::Type *doGetType() const override;
  std::vector<Value *> doGetUsedValues() const override { return {val}; }
  int doReplaceUsedValue(int id, Value *newValue) override;

  Value *doClone() const override;
};

/// Instr representing setting the field of a value.
class InsertInstr : public AcceptorExtend<InsertInstr, Instr> {
private:
  /// the value being manipulated
  Value *lhs;
  /// the field
  std::string field;
  /// the value being inserted
  Value *rhs;

public:
  static const char NodeId;

  /// Constructs a load instruction.
  /// @param lhs the value being manipulated
  /// @param field the field
  /// @param rhs the new value
  /// @param name the instruction's name
  explicit InsertInstr(Value *lhs, std::string field, Value *rhs, std::string name = "")
      : AcceptorExtend(std::move(name)), lhs(lhs), field(std::move(field)), rhs(rhs) {}

  /// @return the left-hand side
  Value *getLhs() { return lhs; }
  /// @return the left-hand side
  const Value *getLhs() const { return lhs; }
  /// Sets the left-hand side.
  /// @param p the new value
  void setLhs(Value *p) { lhs = p; }

  /// @return the right-hand side
  Value *getRhs() { return rhs; }
  /// @return the right-hand side
  const Value *getRhs() const { return rhs; }
  /// Sets the right-hand side.
  /// @param p the new value
  void setRhs(Value *p) { rhs = p; }

  /// @return the field
  const std::string &getField() const { return field; }
  /// Sets the field.
  /// @param f the new field
  void setField(std::string f) { field = std::move(f); }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  const types::Type *doGetType() const override { return lhs->getType(); }
  std::vector<Value *> doGetUsedValues() const override { return {lhs, rhs}; }
  int doReplaceUsedValue(int id, Value *newValue) override;

  Value *doClone() const override;
};

/// Instr representing calling a function.
class CallInstr : public AcceptorExtend<CallInstr, Instr> {
private:
  /// the function
  Value *func;
  /// the arguments
  std::vector<Value *> args;

public:
  static const char NodeId;

  /// Constructs a call instruction.
  /// @param func the function
  /// @param args the arguments
  /// @param name the instruction's name
  CallInstr(Value *func, std::vector<Value *> args, std::string name = "")
      : AcceptorExtend(std::move(name)), func(func), args(std::move(args)) {}

  /// Constructs a call instruction with no arguments.
  /// @param func the function
  /// @param name the instruction's name
  explicit CallInstr(Value *func, std::string name = "")
      : CallInstr(func, {}, std::move(name)) {}

  /// @return the func
  Value *getFunc() { return func; }
  /// @return the func
  const Value *getFunc() const { return func; }
  /// Sets the func.
  /// @param f the new value
  void setFunc(Value *f) { func = f; }

  /// @return an iterator to the first argument
  auto begin() { return args.begin(); }
  /// @return an iterator beyond the last argument
  auto end() { return args.end(); }
  /// @return an iterator to the first argument
  auto begin() const { return args.begin(); }
  /// @return an iterator beyond the last argument
  auto end() const { return args.end(); }

  /// @return a pointer to the first argument
  Value *front() { return args.front(); }
  /// @return a pointer to the last argument
  Value *back() { return args.back(); }
  /// @return a pointer to the first argument
  const Value *front() const { return args.front(); }
  /// @return a pointer to the last argument
  const Value *back() const { return args.back(); }

  /// Inserts an argument at the given position.
  /// @param pos the position
  /// @param v the argument
  /// @return an iterator to the newly added argument
  template <typename It> auto insert(It pos, Value *v) { return args.insert(pos, v); }
  /// Appends an argument.
  /// @param v the argument
  void push_back(Value *v) { args.push_back(v); }

  /// Sets the args.
  /// @param v the new args vector
  void setArgs(std::vector<Value *> v) { args = std::move(v); }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  const types::Type *doGetType() const override;
  std::vector<Value *> doGetUsedValues() const override;
  int doReplaceUsedValue(int id, Value *newValue) override;

  Value *doClone() const override;
};

/// Instr representing allocating an array on the stack.
class StackAllocInstr : public AcceptorExtend<StackAllocInstr, Instr> {
private:
  /// the array type
  types::Type *arrayType;
  /// number of elements to allocate
  int64_t count;

public:
  static const char NodeId;

  /// Constructs a stack allocation instruction.
  /// @param arrayType the type of the array
  /// @param count the number of elements
  /// @param name the name
  StackAllocInstr(types::Type *arrayType, int64_t count, std::string name = "")
      : AcceptorExtend(std::move(name)), arrayType(arrayType), count(count) {}

  /// @return the count
  int64_t getCount() const { return count; }
  /// Sets the count.
  /// @param c the new value
  void setCount(int64_t c) { count = c; }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  const types::Type *doGetType() const override { return arrayType; }

  std::vector<types::Type *> doGetUsedTypes() const override { return {arrayType}; }
  int doReplaceUsedType(const std::string &name, types::Type *newType) override;

  Value *doClone() const override;
};

/// Instr representing getting information about a type.
class TypePropertyInstr : public AcceptorExtend<TypePropertyInstr, Instr> {
public:
  enum Property { IS_ATOMIC, SIZEOF };

private:
  /// the type being inspected
  types::Type *inspectType;
  /// the property being checked
  Property property;

public:
  static const char NodeId;

  /// Constructs a type property instruction.
  /// @param type the type being inspected
  /// @param name the name
  explicit TypePropertyInstr(types::Type *type, Property property,
                             std::string name = "")
      : AcceptorExtend(std::move(name)), inspectType(type), property(property) {}

  /// @return the type being inspected
  const types::Type *getInspectType() const { return inspectType; }

  /// @return the property being inspected
  Property getProperty() const { return property; }
  /// Sets the property.
  /// @param p the new value
  void setProperty(Property p) { property = p; }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  const types::Type *doGetType() const override;
  std::vector<types::Type *> doGetUsedTypes() const override { return {inspectType}; }
  int doReplaceUsedType(const std::string &name, types::Type *newType) override;

  Value *doClone() const override;
};

/// Instr representing a Python yield expression.
class YieldInInstr : public AcceptorExtend<YieldInInstr, Instr> {
private:
  /// the type of the value being yielded in.
  types::Type *type;
  /// whether or not to suspend
  bool suspend;

public:
  static const char NodeId;

  /// Constructs a yield in instruction.
  /// @param type the type of the value being yielded in
  /// @param supsend whether to suspend
  /// @param name the instruction's name
  explicit YieldInInstr(types::Type *type, bool suspend = true, std::string name = "")
      : AcceptorExtend(std::move(name)), type(type), suspend(suspend) {}

  /// @return true if the instruction suspends
  bool isSuspending() const { return suspend; }
  /// Sets the instruction suspending flag.
  /// @param v the new value
  void setSuspending(bool v = true) { suspend = v; }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  const types::Type *doGetType() const override { return type; }

  std::vector<types::Type *> doGetUsedTypes() const override { return {type}; }
  int doReplaceUsedType(const std::string &name, types::Type *newType) override;

  Value *doClone() const override;
};

/// Instr representing a ternary operator.
class TernaryInstr : public AcceptorExtend<TernaryInstr, Instr> {
private:
  /// the condition
  Value *cond;
  /// the true value
  Value *trueValue;
  /// the false value
  Value *falseValue;

public:
  static const char NodeId;

  /// Constructs a ternary instruction.
  /// @param cond the condition
  /// @param trueValue the true value
  /// @param falseValue the false value
  /// @param name the instruction's name
  TernaryInstr(Value *cond, Value *trueValue, Value *falseValue, std::string name = "")
      : AcceptorExtend(std::move(name)), cond(cond), trueValue(trueValue),
        falseValue(falseValue) {}

  /// @return the condition
  Value *getCond() { return cond; }
  /// @return the condition
  const Value *getCond() const { return cond; }
  /// Sets the condition.
  /// @param v the new value
  void setCond(Value *v) { cond = v; }

  /// @return the condition
  Value *getTrueValue() { return trueValue; }
  /// @return the condition
  const Value *getTrueValue() const { return trueValue; }
  /// Sets the true value.
  /// @param v the new value
  void setTrueValue(Value *v) { trueValue = v; }

  /// @return the false value
  Value *getFalseValue() { return falseValue; }
  /// @return the false value
  const Value *getFalseValue() const { return falseValue; }
  /// Sets the value.
  /// @param v the new value
  void setFalseValue(Value *v) { falseValue = v; }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  const types::Type *doGetType() const override { return trueValue->getType(); }
  std::vector<Value *> doGetUsedValues() const override {
    return {cond, trueValue, falseValue};
  }
  int doReplaceUsedValue(int id, Value *newValue) override;

  Value *doClone() const override;
};

/// Base for control flow instructions
class ControlFlowInstr : public AcceptorExtend<ControlFlowInstr, Instr> {
protected:
  /// the target
  Value *target;

public:
  static const char NodeId;

  /// Constructs a control flow instruction.
  /// @param target the flow being targeted
  explicit ControlFlowInstr(Flow *target, std::string name = "")
      : AcceptorExtend(std::move(name)), target(target) {}

  /// @return the target
  Flow *getTarget() { return cast<Flow>(target); }
  /// @return the target
  const Flow *getTarget() const { return cast<Flow>(target); }
  /// Sets the count.
  /// @param f the new value
  void setTarget(Flow *f) { target = f; }

private:
  std::vector<Value *> doGetUsedValues() const override { return {target}; }
  int doReplaceUsedValue(int id, Value *newValue) override;
};

/// Instr representing a break statement.
class BreakInstr : public AcceptorExtend<BreakInstr, ControlFlowInstr> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Instr representing a continue statement.
class ContinueInstr : public AcceptorExtend<ContinueInstr, ControlFlowInstr> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Instr representing a return statement.
class ReturnInstr : public AcceptorExtend<ReturnInstr, ControlFlowInstr> {
private:
  /// the value
  Value *value;

public:
  static const char NodeId;

  explicit ReturnInstr(Value *value = nullptr, std::string name = "")
      : AcceptorExtend(nullptr, std::move(name)), value(value) {}

  /// @return the value
  Value *getValue() { return value; }
  /// @return the value
  const Value *getValue() const { return value; }
  /// Sets the value.
  /// @param v the new value
  void setValue(Value *v) { value = v; }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  std::vector<Value *> doGetUsedValues() const override;
  int doReplaceUsedValue(int id, Value *newValue) override;

  Value *doClone() const override;
};

class YieldInstr : public AcceptorExtend<YieldInstr, Instr> {
private:
  /// the value
  Value *value;

public:
  static const char NodeId;

  explicit YieldInstr(Value *value = nullptr, std::string name = "")
      : AcceptorExtend(std::move(name)), value(value) {}

  /// @return the value
  Value *getValue() { return value; }
  /// @return the value
  const Value *getValue() const { return value; }
  /// Sets the value.
  /// @param v the new value
  void setValue(Value *v) { value = v; }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  std::vector<Value *> doGetUsedValues() const override;
  int doReplaceUsedValue(int id, Value *newValue) override;

  Value *doClone() const override;
};

class ThrowInstr : public AcceptorExtend<ThrowInstr, Instr> {
private:
  /// the value
  Value *value;

public:
  static const char NodeId;

  explicit ThrowInstr(Value *value = nullptr, std::string name = "")
      : AcceptorExtend(std::move(name)), value(value) {}

  /// @return the value
  Value *getValue() { return value; }
  /// @return the value
  const Value *getValue() const { return value; }
  /// Sets the value.
  /// @param v the new value
  void setValue(Value *v) { value = v; }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  std::vector<Value *> doGetUsedValues() const override { return {value}; }
  int doReplaceUsedValue(int id, Value *newValue) override;

  Value *doClone() const override;
};

/// Instr that contains a flow and value.
class FlowInstr : public AcceptorExtend<FlowInstr, Instr> {
private:
  /// the flow
  Value *flow;
  /// the output value
  Value *val;

public:
  static const char NodeId;

  /// Constructs a flow value.
  /// @param flow the flow
  /// @param val the output value
  /// @param name the name
  explicit FlowInstr(Flow *flow, Value *val, std::string name = "")
      : AcceptorExtend(std::move(name)), flow(flow), val(val) {}

  /// @return the flow
  Flow *getFlow() { return cast<Flow>(flow); }
  /// @return the flow
  const Flow *getFlow() const { return cast<Flow>(flow); }
  /// Sets the flow.
  /// @param f the new flow
  void setFlow(Flow *f) { flow = f; }

  /// @return the value
  Value *getValue() { return val; }
  /// @return the value
  const Value *getValue() const { return val; }
  /// Sets the value.
  /// @param v the new value
  void setValue(Value *v) { val = v; }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  const types::Type *doGetType() const override { return val->getType(); }
  std::vector<Value *> doGetUsedValues() const override { return {flow, val}; }
  int doReplaceUsedValue(int id, Value *newValue) override;

  Value *doClone() const override;
};

} // namespace ir
} // namespace seq
