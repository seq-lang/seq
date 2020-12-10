#pragma once

#include <memory>

#include "base.h"
#include "types/types.h"

namespace seq {
namespace ir {

namespace util {
class SIRVisitor;
}
struct Var;

/// SIR object representing an argument to other SIR objects.
struct Operand : public AttributeHolder {
  virtual ~Operand() = default;

  virtual void accept(util::SIRVisitor &v);

  /// @return the operand's type
  virtual types::Type *getType() const = 0;

  std::string referenceString() const override { return "operand"; };
};

using OperandPtr = std::unique_ptr<Operand>;

/// Operand representing the value of a variable.
struct VarOperand : public Operand {
  /// the variable
  Var *var;

  /// Constructs a variable operand.
  /// @param var the variable
  explicit VarOperand(Var *var) : var(var) {}

  void accept(util::SIRVisitor &v) override;

  types::Type *getType() const override;

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Operand representing the address of a variable.
struct VarPointerOperand : public Operand {
  /// the variable
  Var *var;
  /// the pointer type
  types::PointerType *type;

  explicit VarPointerOperand(Var *var, types::PointerType *type)
      : var(var), type(type) {}

  void accept(util::SIRVisitor &v) override;

  types::Type *getType() const override { return type; }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

enum LiteralType { NONE, INT, UINT, FLOAT, BOOL, STR };

/// Operand representing a literal value.
struct LiteralOperand : public Operand {
  LiteralType literalType;
  types::Type *type;
  seq_int_t ival = 0;
  double fval = 0.0;
  bool bval = false;
  std::string sval;

  explicit LiteralOperand(seq_int_t ival, types::Type *type)
      : literalType(LiteralType::INT), type(type), ival(ival) {}
  explicit LiteralOperand(double fval, types::Type *type)
      : literalType(LiteralType::FLOAT), type(type), fval(fval) {}
  explicit LiteralOperand(bool bval, types::Type *type)
      : literalType(LiteralType::BOOL), type(type), bval(bval) {}
  explicit LiteralOperand(std::string sval, types::Type *type)
      : literalType(LiteralType::STR), type(type), sval(std::move(sval)) {}

  void accept(util::SIRVisitor &v) override;

  types::Type *getType() const override { return type; }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

} // namespace ir
} // namespace seq
