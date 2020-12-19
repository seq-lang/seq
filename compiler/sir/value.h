#pragma once

#include "base.h"
#include "types/types.h"

#include "util/fmt/format.h"

namespace seq {
namespace ir {

class Value : public IRNode, public IdMixin {
public:
  /// Constructs a value.
  /// @param the value's name
  explicit Value(std::string name = "") : IRNode(std::move(name)) {}

  virtual ~Value() noexcept = default;

  std::string referenceString() const override {
    return fmt::format(FMT_STRING("{}.{}"), getName(), getId());
  }

  /// @return the value's type
  virtual types::Type *getType() const = 0;
};

using ValuePtr = std::unique_ptr<Value>;

/// Value that contains an unowned value reference.
class ValueProxy : public Value {
private:
  /// the referenced value
  Value *val;

public:
  /// Constructs a value proxy.
  /// @param val the referenced value
  /// @param name the name
  explicit ValueProxy(Value *val, std::string name = "")
      : Value(std::move(name)), val(val) {}

  void accept(util::SIRVisitor &v) override { v.visit(this); }

  types::Type *getType() const override { return val->getType(); }

private:
  std::ostream &doFormat(std::ostream &os) const override {
    return os << val->referenceString();
  }
};

} // namespace ir
} // namespace seq

// See https://github.com/fmtlib/fmt/issues/1283.
namespace fmt {
using seq::ir::Value;

template <typename Char>
struct formatter<Value, Char> : fmt::v6::internal::fallback_formatter<Value, Char> {};
} // namespace fmt
