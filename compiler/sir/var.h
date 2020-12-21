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
class Var : public AcceptorExtend<Var, IRNode>, IdMixin {
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

  types::Type *getType() const { return type; }

  std::string referenceString() const override {
    return fmt::format(FMT_STRING("{}.{}"), getName(), getId());
  }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

using VarPtr = std::unique_ptr<Var>;

/// Value that contains an unowned variable reference.
class VarValue : public AcceptorExtend<VarValue, Value> {
private:
  /// the referenced var
  Var *val;

public:
  static const char NodeId;

  /// Constructs a variable value.
  /// @param val the referenced value
  /// @param name the name
  explicit VarValue(Var *val, std::string name = "")
      : AcceptorExtend(std::move(name)), val(val) {}

  types::Type *getType() const override { return val->getType(); }

private:
  std::ostream &doFormat(std::ostream &os) const override {
    return os << val->referenceString();
  }
};

/// Value that represents a pointer.
class PointerValue : public AcceptorExtend<PointerValue, Value> {
private:
  /// the referenced var
  Var *val;

public:
  static const char NodeId;

  /// Constructs a variable value.
  /// @param val the referenced value
  /// @param name the name
  explicit PointerValue(Var *val, std::string name = "")
      : AcceptorExtend(std::move(name)), val(val) {}

  types::Type *getType() const override;

private:
  std::ostream &doFormat(std::ostream &os) const override {
    return os << '&' << val->referenceString();
  }
};


} // namespace ir
} // namespace seq

// See https://github.com/fmtlib/fmt/issues/1283.
namespace fmt {
using seq::ir::Var;

template <typename Char>
struct formatter<Var, Char> : fmt::v6::internal::fallback_formatter<Var, Char> {};
} // namespace fmt