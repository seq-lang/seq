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
  virtual const types::Type *getType() const = 0;

  /// @return a clone of the value
  std::unique_ptr<Value> clone() const { return std::unique_ptr<Value>(doClone()); }

protected:
  virtual Value *doClone() const = 0;
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

  const types::Type *getType() const override { return val->getType(); }

  /// @return the value
  Value *getValue() { return val; }
  /// @return the value
  const Value *getValue() const { return val; }
  /// Sets the value.
  /// @param v the new value
  void setValue(Value *v) { val = v; }

private:
  std::ostream &doFormat(std::ostream &os) const override {
    return os << val->referenceString();
  }

  Value *doClone() const override;
};

} // namespace ir
} // namespace seq

// See https://github.com/fmtlib/fmt/issues/1283.
namespace fmt {
using seq::ir::Value;

template <typename Char>
struct formatter<Value, Char> : fmt::v6::internal::fallback_formatter<Value, Char> {};
} // namespace fmt
