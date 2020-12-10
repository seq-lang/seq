#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base.h"
#include "types/types.h"
#include "util/common.h"

namespace seq {
namespace ir {

namespace util {
class SIRVisitor;
}

/// SIR object representing a variable.
struct Var : public AttributeHolder {
  /// the variable's type
  types::Type *type;
  /// true if the variable is global, false otherwise
  bool global;

  /// Constructs a variable.
  /// @param name the variable's name
  /// @param type the variable's type
  /// @param global true if global, false otherwise
  Var(std::string name, types::Type *type, bool global = false)
      : AttributeHolder(std::move(name)), type(type), global(global) {}

  /// Constructs an unnamed variable.
  /// @param type the variable's type
  /// @param global true if global, false otherwise
  explicit Var(types::Type *type, bool global = false) : Var("var", type, global) {}

  virtual ~Var() = default;

  virtual void accept(util::SIRVisitor &v);

  /// @return true if vars are equal
  bool is(Var *other) const { return other && name == other->name; }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

using VarPtr = std::unique_ptr<Var>;

} // namespace ir
} // namespace seq
