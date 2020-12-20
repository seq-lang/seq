#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "types/types.h"
#include "util/common.h"
#include "value.h"

namespace seq {
namespace ir {

/// SIR object representing a variable.
class Var : public AcceptorExtend<Var, Value> {
private:
  /// the variable's type
  types::Type *type;

public:
  static const char NodeId;

  /// Constructs a variable.
  /// @param name the variable's name
  /// @param type the variable's type
  explicit Var(types::Type *type, std::string name = "")
      : AcceptorExtend(std::move(name)), type(type) {}

  types::Type *getType() const override;

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

using VarPtr = std::unique_ptr<Var>;

} // namespace ir
} // namespace seq
