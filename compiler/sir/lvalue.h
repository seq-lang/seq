#pragma once

#include <memory>
#include <string>

#include "types/types.h"

#include "base.h"

namespace seq {
namespace ir {

namespace util {
class SIRVisitor;
}
struct Var;

/// SIR object representing the left-hand side of an assignment (a memory location).
struct Lvalue : public AttributeHolder {
  virtual ~Lvalue() = default;

  virtual void accept(util::SIRVisitor &v);

  /// @return the lvalue's type
  virtual types::Type *getType() const = 0;

  std::string referenceString() const override { return "lvalue"; };
};

using LvaluePtr = std::unique_ptr<Lvalue>;

/// Lvalue representing a variable.
struct VarLvalue : public Lvalue {
  /// the variable
  Var *var;

  /// Constructs a var lvalue.
  /// @param var the variable
  explicit VarLvalue(Var *var) : var(var) {}

  void accept(util::SIRVisitor &v) override;

  types::Type *getType() const override;

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Lvalue representing a particular field of a variable.
struct VarMemberLvalue : public Lvalue {
  /// the variable
  Var *var;
  /// the field
  std::string field;

  /// Constructs a var member lvalue
  VarMemberLvalue(Var *var, std::string field) : var(var), field(std::move(field)) {}

  void accept(util::SIRVisitor &v) override;

  types::Type *getType() const override;

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

} // namespace ir
} // namespace seq
