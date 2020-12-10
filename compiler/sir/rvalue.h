#pragma once

#include <memory>
#include <utility>

#include "types/types.h"

#include "base.h"
#include "operand.h"

namespace seq {
namespace ir {

namespace util {
class SIRVisitor;
}

/// SIR object representing the right hand side of an assignment.
struct Rvalue : public AttributeHolder {
  virtual ~Rvalue() = default;

  virtual void accept(util::SIRVisitor &v);

  /// @return the rvalue's type
  virtual types::Type *getType() const = 0;

  std::string referenceString() const override { return "rvalue"; };
};

using RvaluePtr = std::unique_ptr<Rvalue>;

/// Rvalue representing the member of an operand.
struct MemberRvalue : public Rvalue {
  /// the "variable"
  OperandPtr var;
  /// the field
  std::string field;

  /// Constructs a member rvalue.
  /// @param the var
  /// @param the field
  MemberRvalue(OperandPtr var, std::string field);

  void accept(util::SIRVisitor &v) override;

  types::Type *getType() const override;

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Rvalue representing a function call.
struct CallRvalue : public Rvalue {
  /// the function
  OperandPtr func;
  /// the arguments
  std::vector<OperandPtr> args;

  /// Constructs a call rvalue.
  /// @param func the function
  /// @param args the arguments
  CallRvalue(OperandPtr func, std::vector<OperandPtr> args);

  /// Constructs a call rvalue with no arguments.
  /// @param func the function
  explicit CallRvalue(OperandPtr func) : CallRvalue(std::move(func), {}) {}

  void accept(util::SIRVisitor &v) override;

  types::Type *getType() const override;

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Rvalue representing an operand. May be a literal, variable value, etc.
struct OperandRvalue : public Rvalue {
  /// the operand
  OperandPtr operand;

  /// Constructs an operand rvalue.
  /// @param operand the operand
  explicit OperandRvalue(OperandPtr operand);

  void accept(util::SIRVisitor &v) override;

  types::Type *getType() const override;

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Rvalue representing allocating an array on the stack.
struct StackAllocRvalue : public Rvalue {
  /// the array type
  types::ArrayType *arrayType;
  /// number of elements to allocate
  OperandPtr count;

  /// Constructs a stack allocation rvalue.
  /// @param arrayType the type of the array
  /// @param count the number of elements
  StackAllocRvalue(types::ArrayType *arrayType, OperandPtr count);

  void accept(util::SIRVisitor &v) override;

  types::Type *getType() const override { return arrayType; }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

struct YieldInRvalue : public Rvalue {
  types::Type *type;

  explicit YieldInRvalue(types::Type *type) : type(type) {}

  void accept(util::SIRVisitor &v) override;

  types::Type *getType() const override { return type; }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

} // namespace ir
} // namespace seq