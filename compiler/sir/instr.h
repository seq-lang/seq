#pragma once

#include <memory>
#include <string>

#include "flow.h"
#include "types/types.h"
#include "value.h"

namespace seq {
namespace ir {

namespace util {
class SIRVisitor;
}

/// SIR object representing an "instruction," or discrete operation in the context of a
/// block.
class Instr : public Value {
public:
  /// Constructs an instruction.
  /// @param name the name
  explicit Instr(std::string name) : Value(std::move(name)) {}

  virtual ~Instr() = default;

  types::Type *getType() const override { return nullptr; }
};

/// CRTP base for instructions that provides visitor functionality.
template <typename Derived, typename Base = Instr> class InstrBase : public Base {
public:
  /// Constructs an instruction.
  /// @tparam Args the arguments
  template <typename... Args>
  explicit InstrBase(Args... args) : Base(std::forward<Args>(args)...) {}

  virtual ~InstrBase() = default;

  void accept(util::SIRVisitor &v) override { v.visit(static_cast<Derived *>(this)); }
};

/// Instr representing setting a memory location.
class AssignInstr : public InstrBase<AssignInstr> {
private:
  /// the left-hand side
  ValuePtr lhs;
  /// the right-hand side
  ValuePtr rhs;
  /// the optional field
  std::string field;

public:
  /// Constructs an assign instruction.
  /// @param lhs the left-hand side
  /// @param rhs the right-hand side
  /// @param field the field being set, may be empty
  /// @param name the instruction's name
  AssignInstr(ValuePtr lhs, ValuePtr rhs, std::string field = "", std::string name = "")
      : InstrBase(std::move(name)), lhs(std::move(lhs)), rhs(std::move(rhs)),
        field(std::move(field)) {}

  /// @return the left-hand side
  const ValuePtr &getLhs() const { return lhs; }
  /// Sets the left-hand side
  /// @param l the new value
  void setLhs(ValuePtr v) { lhs = std::move(v); }

  /// @return the right-hand side
  const ValuePtr &getRhs() const { return rhs; }
  /// Sets the right-hand side
  /// @param l the new value
  void setRhs(ValuePtr v) { rhs = std::move(v); }

  /// @return the field
  const std::string &getField() const { return field; }
  /// Sets the field.
  /// @param f the new value
  void setField(std::string f) { field = std::move(f); }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Instr representing loading a memory location.
class LoadInstr : public InstrBase<LoadInstr> {
private:
  /// the location
  ValuePtr ptr;
  /// the field
  std::string field;

public:
  /// Constructs a load instruction.
  /// @param ptr the location
  /// @param field the field being set, may be empty
  /// @param name the instruction's name
  explicit LoadInstr(ValuePtr ptr, std::string field = "", std::string name = "")
      : InstrBase(std::move(name)), ptr(std::move(ptr)), field(std::move(field)) {}

  types::Type *getType() const override;

  /// @return the location
  const ValuePtr &getPtr() const { return ptr; }
  /// Sets the location.
  /// @param p the new value
  void setPtr(ValuePtr p) { ptr = std::move(p); }

  /// @return the field
  const std::string &getField() const { return field; }
  /// Sets the field.
  /// @param p the new value
  void setField(std::string f) { field = std::move(f); }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Instr representing calling a function.
class CallInstr : public InstrBase<CallInstr> {
public:
  using iterator = std::vector<ValuePtr>::iterator;
  using const_iterator = std::vector<ValuePtr>::const_iterator;
  using reference = std::vector<ValuePtr>::reference;
  using const_reference = std::vector<ValuePtr>::const_reference;

private:
  /// the function
  ValuePtr func;
  /// the arguments
  std::vector<ValuePtr> args;

public:
  /// Constructs a call instruction.
  /// @param func the function
  /// @param args the arguments
  /// @param name the instruction's name
  CallInstr(ValuePtr func, std::vector<ValuePtr> args, std::string name = "")
      : InstrBase(std::move(name)), func(std::move(func)), args(std::move(args)) {}

  /// Constructs a call instruction with no arguments.
  /// @param func the function
  /// @param name the instruction's name
  explicit CallInstr(ValuePtr func, std::string name = "")
      : CallInstr(std::move(func), {}, std::move(name)) {}

  types::Type *getType() const override;

  /// @return an iterator to the first argument
  iterator begin() { return args.begin(); }
  /// @return an iterator beyond the last argument
  iterator end() { return args.end(); }
  /// @return an iterator to the first argument
  const_iterator begin() const { return args.begin(); }
  /// @return an iterator beyond the last argument
  const_iterator end() const { return args.end(); }

  /// @return a reference to the first argument
  reference front() { return args.front(); }
  /// @return a reference to the last argument
  reference back() { return args.back(); }
  /// @return a reference to the first argument
  const_reference front() const { return args.front(); }
  /// @return a reference to the last argument
  const_reference back() const { return args.back(); }

  /// Inserts an argument at the given position.
  /// @param pos the position
  /// @param v the argument
  /// @return an iterator to the newly added argument
  iterator insert(iterator pos, ValuePtr v) { return args.insert(pos, std::move(v)); }
  /// Inserts an argument at the given position.
  /// @param pos the position
  /// @param v the argument
  /// @return an iterator to the newly added argument
  iterator insert(const_iterator pos, ValuePtr v) {
    return args.insert(pos, std::move(v));
  }
  /// Appends an argument.
  /// @param v the argument
  void push_back(ValuePtr v) { args.push_back(std::move(v)); }

  /// Sets the args.
  /// @param v the new args vector
  void setArgs(std::vector<ValuePtr> v) { args = std::move(v); }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Instr representing allocating an array on the stack.
class StackAllocInstr : public InstrBase<StackAllocInstr> {
private:
  /// the array type
  types::Type *arrayType;
  /// number of elements to allocate
  ValuePtr count;

public:
  /// Constructs a stack allocation instruction.
  /// @param arrayType the type of the array
  /// @param count the number of elements
  StackAllocInstr(types::Type *arrayType, ValuePtr count, std::string name = "")
      : InstrBase(std::move(name)), arrayType(arrayType), count(std::move(count)) {}

  types::Type *getType() const override { return arrayType; }

  /// @return the count
  const ValuePtr &getCount() const { return count; }
  /// Sets the count.
  /// @param c the new value
  void setCount(ValuePtr c) { count = std::move(c); }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Instr representing a Python yield expression.
class YieldInInstr : public InstrBase<YieldInInstr> {
private:
  /// @param the type of the value being yielded in.
  types::Type *type;

public:
  /// Constructs a yield in instruction.
  /// @param type the type of the value being yielded in
  /// @param name the instruction's name
  explicit YieldInInstr(types::Type *type, std::string name = "")
      : InstrBase(std::move(name)), type(type) {}

  types::Type *getType() const override { return type; }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Instr representing a ternary operator.
class TernaryInstr : public InstrBase<TernaryInstr> {
private:
  /// the condition
  ValuePtr cond;
  /// the true value
  ValuePtr trueValue;
  /// the false value
  ValuePtr falseValue;

public:
  /// Constructs a ternary instruction.
  /// @param cond the condition
  /// @param trueValue the true value
  /// @param falseValue the false value
  /// @param name the instruction's name
  TernaryInstr(ValuePtr cond, ValuePtr trueValue, ValuePtr falseValue,
               std::string name = "")
      : InstrBase(std::move(name)), cond(std::move(cond)),
        trueValue(std::move(trueValue)), falseValue(std::move(falseValue)) {}

  types::Type *getType() const override { return trueValue->getType(); }

  /// @return the condition
  const ValuePtr &getCond() const { return cond; }
  /// Sets the condition.
  /// @param v the new value
  void setCond(ValuePtr v) { cond = std::move(v); }

  /// @return the condition
  const ValuePtr &getTrueValue() const { return trueValue; }
  /// Sets the true value.
  /// @param v the new value
  void setTrueValue(ValuePtr v) { trueValue = std::move(v); }

  /// @return the false value
  const ValuePtr &getFalseValue() const { return falseValue; }
  /// Sets the value.
  /// @param v the new value
  void setFalseValue(ValuePtr v) { falseValue = std::move(v); }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Base for control flow instructions
class ControlFlowInstr : public Instr {
private:
  /// the target
  ValuePtr target;

public:
  /// Constructs a control flow instruction.
  /// @param target the flow being targeted
  explicit ControlFlowInstr(ValuePtr target, std::string name = "")
      : Instr(std::move(name)), target(std::move(target)) {}

  /// @return the target
  const ValuePtr &getTarget() const { return target; }
  /// Sets the count.
  /// @param f the new value
  void setTarget(ValuePtr f) { target = std::move(f); }
};

/// Instr representing a break statement.
class BreakInstr : public InstrBase<BreakInstr, ControlFlowInstr> {
public:
  /// Constructs a break instruction.
  /// @param target the flow being broken out of
  /// @param name the instruction's name
  explicit BreakInstr(ValuePtr target, std::string name = "")
      : InstrBase(std::move(target), std::move(name)) {}

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Instr representing a continue statement.
class ContinueInstr : public InstrBase<ContinueInstr, ControlFlowInstr> {
public:
  /// Constructs a continue instruction.
  /// @param target the flow being broken out of
  /// @param name the instruction's name
  explicit ContinueInstr(ValuePtr target, std::string name = "")
      : InstrBase(std::move(target), std::move(name)) {}

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Instr representing a return statement.
class ReturnInstr : public InstrBase<ReturnInstr, ControlFlowInstr> {
private:
  /// the value
  ValuePtr value;

public:
  explicit ReturnInstr(ValuePtr value = nullptr, std::string name = "")
      : InstrBase(nullptr, std::move(name)), value(std::move(value)) {}

  /// @return the value
  const ValuePtr &getValue() const { return value; }
  /// Sets the value.
  /// @param v the new value
  void setValue(ValuePtr v) { value = std::move(v); }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

class YieldInstr : public InstrBase<YieldInstr> {
private:
  /// the value
  ValuePtr value;

public:
  explicit YieldInstr(ValuePtr value = nullptr, std::string name = "")
      : InstrBase(std::move(name)), value(std::move(value)) {}

  /// @return the value
  const ValuePtr &getValue() const { return value; }
  /// Sets the value.
  /// @param v the new value
  void setValue(ValuePtr v) { value = std::move(v); }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

class ThrowInstr : public InstrBase<ThrowInstr> {
private:
  /// the value
  ValuePtr value;

public:
  explicit ThrowInstr(ValuePtr value = nullptr, std::string name = "")
      : InstrBase(std::move(name)), value(std::move(value)) {}

  /// @return the value
  const ValuePtr &getValue() const { return value; }
  /// Sets the value.
  /// @param v the new value
  void setValue(ValuePtr v) { value = std::move(v); }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

class AssertInstr : public InstrBase<AssertInstr> {
private:
  /// the value
  ValuePtr value;
  /// the message
  std::string msg;

public:
  explicit AssertInstr(ValuePtr value = nullptr, std::string msg = "",
                       std::string name = "")
      : InstrBase(std::move(name)), value(std::move(value)), msg(std::move(msg)) {}

  /// @return the value
  const ValuePtr &getValue() const { return value; }
  /// Sets the value.
  /// @param v the new value
  void setValue(ValuePtr v) { value = std::move(v); }

  /// @return the message
  const std::string &getMsg() const { return msg; }
  /// Sets the message
  /// @param m the new message
  void setMessage(std::string m) { msg = std::move(m); }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Instr that contains a flow and value.
class FlowInstr : public InstrBase<FlowInstr> {
private:
  /// the flow
  ValuePtr flow;
  /// the output value
  ValuePtr val;

public:
  /// Constructs a flow value.
  /// @param flow the flow
  /// @param val the output value
  /// @param name the name
  explicit FlowInstr(ValuePtr flow, ValuePtr val, std::string name = "")
      : InstrBase(std::move(name)), flow(std::move(flow)), val(std::move(val)) {}

  void accept(util::SIRVisitor &v) override { v.visit(this); }

  types::Type *getType() const override { return val->getType(); }

  /// @return the flow
  const ValuePtr &getFlow() const { return flow; }
  /// Sets the flow.
  /// @param f the new flow
  void setFlow(ValuePtr f) { flow = std::move(f); }

  /// @return the value
  const ValuePtr &getValue() const { return val; }
  /// Sets the value.
  /// @param v the new value
  void setValue(ValuePtr v) { val = std::move(v); }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

} // namespace ir
} // namespace seq
