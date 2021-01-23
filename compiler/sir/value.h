#pragma once

#include "base.h"
#include "types/types.h"

#include "util/fmt/format.h"

namespace seq {
namespace ir {

class Func;

class Value : public AcceptorExtend<Value, IRNode>,
              public IdMixin,
              public ParentFuncMixin {
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
  Value *clone() const;

protected:
  virtual Value *doClone() const = 0;
};

} // namespace ir
} // namespace seq

// See https://github.com/fmtlib/fmt/issues/1283.
namespace fmt {
using seq::ir::Value;

template <typename Char>
struct formatter<Value, Char> : fmt::v6::internal::fallback_formatter<Value, Char> {};
} // namespace fmt
