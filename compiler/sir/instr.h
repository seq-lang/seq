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

  virtual ~Instr() = default;

  const types::Type *getType() const override { return nullptr; }
};

/// Instr representing setting a memory location.
class AssignInstr : public AcceptorExtend<AssignInstr, Instr> {
private:
  /// the left-hand side
  Var *lhs;
  /// the right-hand side
  ValuePtr rhs;

public:
  static const char NodeId;

  /// Constructs an assign instruction.
  /// @param lhs the left-hand side
  /// @param rhs the right-hand side
  /// @param field the field being set, may be empty
  /// @param name the instruction's name
  AssignInstr(Var *lhs, ValuePtr rhs, std::string name = "")
      : AcceptorExtend(std::move(name)), lhs(lhs), rhs(std::move(rhs)) {}

  /// @return the left-hand side
  Var *getLhs() { return lhs; }
  /// @return the left-hand side
  const Var *getLhs() const { return lhs; }
  /// Sets the left-hand side
  /// @param l the new value
  void setLhs(Var *v) { lhs = v; }

  /// @return the right-hand side
  Value *getRhs() { return rhs.get(); }
  /// @return the right-hand side
  const Value *getRhs() const { return rhs.get(); }
  /// Sets the right-hand side
  /// @param l the new value
  void setRhs(ValuePtr v) { rhs = std::move(v); }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Instr representing loading the field of a value.
class ExtractInstr : public AcceptorExtend<ExtractInstr, Instr> {
private:
  /// the value being manipulated
  ValuePtr val;
  /// the field
  std::string field;

public:
  static const char NodeId;

  /// Constructs a load instruction.
  /// @param val the value being manipulated
  /// @param field the field
  /// @param name the instruction's name
  explicit ExtractInstr(ValuePtr val, std::string field, std::string name = "")
      : AcceptorExtend(std::move(name)), val(std::move(val)), field(std::move(field)) {}

  const types::Type *getType() const override;

  /// @return the location
  Value *getVal() { return val.get(); }
  /// @return the location
  const Value *getVal() const { return val.get(); }
  /// Sets the location.
  /// @param p the new value
  void setVal(ValuePtr p) { val = std::move(p); }

  /// @return the field
  const std::string &getField() { return field; }
  /// Sets the field.
  /// @param f the new field
  void setField(std::string f) { field = std::move(f); }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Instr representing setting the field of a value.
class InsertInstr : public AcceptorExtend<InsertInstr, Instr> {
private:
  /// the value being manipulated
  ValuePtr lhs;
  /// the field
  std::string field;
  /// the value being inserted
  ValuePtr rhs;

public:
  static const char NodeId;

  /// Constructs a load instruction.
  /// @param lhs the value being manipulated
  /// @param field the field
  /// @param rhs the new value
  /// @param name the instruction's name
  explicit InsertInstr(ValuePtr lhs, std::string field, ValuePtr rhs,
                       std::string name = "")
      : AcceptorExtend(std::move(name)), lhs(std::move(lhs)), field(std::move(field)),
        rhs(std::move(rhs)) {}

  const types::Type *getType() const override { return lhs->getType(); }

  /// @return the left-hand side
  Value *getLhs() { return lhs.get(); }
  /// @return the left-hand side
  const Value *getLhs() const { return lhs.get(); }
  /// Sets the left-hand side.
  /// @param p the new value
  void setLhs(ValuePtr p) { lhs = std::move(p); }

  /// @return the right-hand side
  Value *getRhs() { return rhs.get(); }
  /// @return the right-hand side
  const Value *getRhs() const { return rhs.get(); }
  /// Sets the right-hand side.
  /// @param p the new value
  void setRhs(ValuePtr p) { rhs = std::move(p); }

  /// @return the field
  const std::string &getField() { return field; }
  /// Sets the field.
  /// @param f the new field
  void setField(std::string f) { field = std::move(f); }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Instr representing calling a function.
class CallInstr : public AcceptorExtend<CallInstr, Instr> {
private:
  /// the function
  ValuePtr func;
  /// the arguments
  std::vector<ValuePtr> args;

public:
  static const char NodeId;

  /// Constructs a call instruction.
  /// @param func the function
  /// @param args the arguments
  /// @param name the instruction's name
  CallInstr(ValuePtr func, std::vector<ValuePtr> args, std::string name = "")
      : AcceptorExtend(std::move(name)), func(std::move(func)), args(std::move(args)) {}

  /// Constructs a call instruction with no arguments.
  /// @param func the function
  /// @param name the instruction's name
  explicit CallInstr(ValuePtr func, std::string name = "")
      : CallInstr(std::move(func), {}, std::move(name)) {}

  const types::Type *getType() const override;

  /// @return the func
  Value *getFunc() { return func.get(); }
  /// @return the func
  const Value *getFunc() const { return func.get(); }
  /// Sets the func.
  /// @param f the new value
  void setFunc(ValuePtr f) { func = std::move(f); }

  /// @return an iterator to the first argument
  auto begin() { return util::raw_ptr_adaptor(args.begin()); }
  /// @return an iterator beyond the last argument
  auto end() { return util::raw_ptr_adaptor(args.end()); }
  /// @return an iterator to the first argument
  auto begin() const { return util::const_raw_ptr_adaptor(args.begin()); }
  /// @return an iterator beyond the last argument
  auto end() const { return util::const_raw_ptr_adaptor(args.end()); }

  /// @return a pointer to the first argument
  Value *front() { return args.front().get(); }
  /// @return a pointer to the last argument
  Value *back() { return args.back().get(); }
  /// @return a pointer to the first argument
  const Value *front() const { return args.front().get(); }
  /// @return a pointer to the last argument
  const Value *back() const { return args.back().get(); }

  /// Inserts an argument at the given position.
  /// @param pos the position
  /// @param v the argument
  /// @return an iterator to the newly added argument
  template <typename It> auto insert(It pos, ValuePtr v) {
    return util::raw_ptr_adaptor(args.insert(pos.internal, std::move(v)));
  }
  /// Appends an argument.
  /// @param v the argument
  void push_back(ValuePtr v) { args.push_back(std::move(v)); }

  /// Sets the args.
  /// @param v the new args vector
  void setArgs(std::vector<ValuePtr> v) { args = std::move(v); }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Instr representing allocating an array on the stack.
class StackAllocInstr : public AcceptorExtend<StackAllocInstr, Instr> {
private:
  /// the array type
  const types::Type *arrayType;
  /// number of elements to allocate
  ValuePtr count;

public:
  static const char NodeId;

  /// Constructs a stack allocation instruction.
  /// @param arrayType the type of the array
  /// @param count the number of elements
  /// @param name the name
  StackAllocInstr(const types::Type *arrayType, ValuePtr count, std::string name = "")
      : AcceptorExtend(std::move(name)), arrayType(arrayType), count(std::move(count)) {
  }

  const types::Type *getType() const override { return arrayType; }

  /// @return the count
  Value *getCount() { return count.get(); }
  /// @return the count
  const Value *getCount() const { return count.get(); }
  /// Sets the count.
  /// @param c the new value
  void setCount(ValuePtr c) { count = std::move(c); }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Instr representing getting information about a type.
class TypePropertyInstr : public AcceptorExtend<TypePropertyInstr, Instr> {
public:
  enum Property { IS_ATOMIC, SIZEOF };

private:
  /// the type being inspected
  const types::Type *type;
  /// the property being checked
  Property property;

public:
  static const char NodeId;

  /// Constructs a type property instruction.
  /// @param type the type being inspected
  /// @param name the name
  explicit TypePropertyInstr(const types::Type *type, Property property = Property::IS_ATOMIC, std::string name = "")
      : AcceptorExtend(std::move(name)), type(type), property(property) {
  }

  const types::Type *getType() const override;

  /// @return the property being inspected
  Property getProperty() const { return property; }
  /// Sets the property.
  /// @param p the new value
  void setProperty(Property p) { property = p; }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};


/// Instr representing a Python yield expression.
class YieldInInstr : public AcceptorExtend<YieldInInstr, Instr> {
private:
  /// the type of the value being yielded in.
  const types::Type *type;
  /// whether or not to suspend
  bool suspend;

public:
  static const char NodeId;

  /// Constructs a yield in instruction.
  /// @param type the type of the value being yielded in
  /// @param supsend whether to suspend
  /// @param name the instruction's name
  explicit YieldInInstr(const types::Type *type, bool suspend = true,
                        std::string name = "")
      : AcceptorExtend(std::move(name)), type(type), suspend(suspend) {}

  const types::Type *getType() const override { return type; }

  /// @return true if the instruction suspends
  bool isSuspending() const { return suspend; }
  /// Sets the instruction suspending flag.
  /// @param v the new value
  void setSuspending(bool v = true) { suspend = v; }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Instr representing a ternary operator.
class TernaryInstr : public AcceptorExtend<TernaryInstr, Instr> {
private:
  /// the condition
  ValuePtr cond;
  /// the true value
  ValuePtr trueValue;
  /// the false value
  ValuePtr falseValue;

public:
  static const char NodeId;

  /// Constructs a ternary instruction.
  /// @param cond the condition
  /// @param trueValue the true value
  /// @param falseValue the false value
  /// @param name the instruction's name
  TernaryInstr(ValuePtr cond, ValuePtr trueValue, ValuePtr falseValue,
               std::string name = "")
      : AcceptorExtend(std::move(name)), cond(std::move(cond)),
        trueValue(std::move(trueValue)), falseValue(std::move(falseValue)) {}

  const types::Type *getType() const override { return trueValue->getType(); }

  /// @return the condition
  Value *getCond() { return cond.get(); }
  /// @return the condition
  const Value *getCond() const { return cond.get(); }
  /// Sets the condition.
  /// @param v the new value
  void setCond(ValuePtr v) { cond = std::move(v); }

  /// @return the condition
  Value *getTrueValue() { return trueValue.get(); }
  /// @return the condition
  const Value *getTrueValue() const { return trueValue.get(); }
  /// Sets the true value.
  /// @param v the new value
  void setTrueValue(ValuePtr v) { trueValue = std::move(v); }

  /// @return the false value
  Value *getFalseValue() { return falseValue.get(); }
  /// @return the false value
  const Value *getFalseValue() const { return falseValue.get(); }
  /// Sets the value.
  /// @param v the new value
  void setFalseValue(ValuePtr v) { falseValue = std::move(v); }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Base for control flow instructions
class ControlFlowInstr : public AcceptorExtend<ControlFlowInstr, Instr> {

protected:
  /// the target
  Flow *target;

public:
  static const char NodeId;

  /// Constructs a control flow instruction.
  /// @param target the flow being targeted
  explicit ControlFlowInstr(Flow *target, std::string name = "")
      : AcceptorExtend(std::move(name)), target(target) {}

  /// @return the target
  Flow *getTarget() { return target; }
  /// @return the target
  const Flow *getTarget() const { return target; }
  /// Sets the count.
  /// @param f the new value
  void setTarget(Flow *f) { target = f; }
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
  ValuePtr value;

public:
  static const char NodeId;

  explicit ReturnInstr(ValuePtr value = nullptr, std::string name = "")
      : AcceptorExtend(nullptr, std::move(name)), value(std::move(value)) {}

  /// @return the value
  Value *getValue() { return value.get(); }
  /// @return the value
  const Value *getValue() const { return value.get(); }
  /// Sets the value.
  /// @param v the new value
  void setValue(ValuePtr v) { value = std::move(v); }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

class YieldInstr : public AcceptorExtend<YieldInstr, Instr> {
private:
  /// the value
  ValuePtr value;

public:
  static const char NodeId;

  explicit YieldInstr(ValuePtr value = nullptr, std::string name = "")
      : AcceptorExtend(std::move(name)), value(std::move(value)) {}

  /// @return the value
  Value *getValue() { return value.get(); }
  /// @return the value
  const Value *getValue() const { return value.get(); }
  /// Sets the value.
  /// @param v the new value
  void setValue(ValuePtr v) { value = std::move(v); }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

class ThrowInstr : public AcceptorExtend<ThrowInstr, Instr> {
private:
  /// the value
  ValuePtr value;

public:
  static const char NodeId;

  explicit ThrowInstr(ValuePtr value = nullptr, std::string name = "")
      : AcceptorExtend(std::move(name)), value(std::move(value)) {}

  /// @return the value
  Value *getValue() { return value.get(); }
  /// @return the value
  const Value *getValue() const { return value.get(); }
  /// Sets the value.
  /// @param v the new value
  void setValue(ValuePtr v) { value = std::move(v); }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Instr that contains a flow and value.
class FlowInstr : public AcceptorExtend<FlowInstr, Instr> {
private:
  /// the flow
  FlowPtr flow;
  /// the output value
  ValuePtr val;

public:
  static const char NodeId;

  /// Constructs a flow value.
  /// @param flow the flow
  /// @param val the output value
  /// @param name the name
  explicit FlowInstr(FlowPtr flow, ValuePtr val, std::string name = "")
      : AcceptorExtend(std::move(name)), flow(std::move(flow)), val(std::move(val)) {}

  const types::Type *getType() const override { return val->getType(); }

  /// @return the flow
  Flow *getFlow() { return flow.get(); }
  /// @return the flow
  const Flow *getFlow() const { return flow.get(); }
  /// Sets the flow.
  /// @param f the new flow
  void setFlow(FlowPtr f) { flow = std::move(f); }

  /// @return the value
  Value *getValue() { return val.get(); }
  /// @return the value
  const Value *getValue() const { return val.get(); }
  /// Sets the value.
  /// @param v the new value
  void setValue(ValuePtr v) { val = std::move(v); }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

} // namespace ir
} // namespace seq
