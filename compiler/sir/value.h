#pragma once

#include "base.h"
#include "types/types.h"

#include "util/fmt/format.h"

namespace seq {
namespace ir {

class Value : public AcceptorExtend<Value, IRNode>, public IdMixin {
public:
  static const char NodeId;

  /// Constructs a value.
  /// @param the value's name
  explicit Value(std::string name = "") : AcceptorExtend(std::move(name)) {}

  virtual ~Value() noexcept = default;

  std::string referenceString() const override {
    return fmt::format(FMT_STRING("{}.{}"), getName(), getId());
  }

  /// @return the value's type
  virtual types::Type *getType() const = 0;
};

using ValuePtr = std::unique_ptr<Value>;

/// Value that contains an unowned value reference.
class ValueProxy : public AcceptorExtend<ValueProxy, Value> {
private:
  /// the referenced value
  Value *val;

public:
  static const char NodeId;

  /// Constructs a value proxy.
  /// @param val the referenced value
  /// @param name the name
  explicit ValueProxy(Value *val, std::string name = "")
      : AcceptorExtend(std::move(name)), val(val) {}

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
