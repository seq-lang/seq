#pragma once

#include "base.h"
#include "types/types.h"

#include "util/fmt/format.h"
#include "util/fmt/ostream.h"

namespace seq {
namespace ir {

class Func;

class Value : public AcceptorExtend<Value, IRNode>,
              public IdMixin,
              public ParentFuncMixin {
private:
  /// lazy replacement
  Value *replacement = nullptr;

public:
  static const char NodeId;

  /// Constructs a value.
  /// @param the value's name
  explicit Value(std::string name = "") : AcceptorExtend(std::move(name)) {}

  virtual ~Value() noexcept = default;

  std::string referenceString() const override {
    if (replacement)
      return replacement->referenceString();
    return fmt::format(FMT_STRING("{}.{}"), getName(), getId());
  }

  bool isConvertible(const void *other) const override {
    return replacement ? replacement->isConvertible(other)
                       : AcceptorExtend::isConvertible(other);
  }

  /// @return the value's type
  types::Type *getType() { return const_cast<types::Type *>(doGetType()); }
  /// @return the value's type
  const types::Type *getType() const { return doGetType(); }

  /// @return a clone of the value
  Value *clone() const;

  /// Lazily replaces all instances of the current value.
  /// @param newValue the new value.
  void replaceAll(Value *newValue) { replacement = newValue; }
  /// @return the actual value after replacements
  Value *getActual() { return replacement ? replacement->getActual() : this; }
  /// @return the actual value after replacements
  const Value *getActual() const {
    return replacement ? replacement->getActual() : this;
  }
  /// @return true if the value has been replaced
  bool isReplaced() const { return replacement != nullptr; }

  /// @return a vector of all the node's children
  std::vector<Value *> getChildren() {
    return replacement ? replacement->getChildren() : doGetChildren();
  }
  /// @return a vector of all the node's children
  std::vector<const Value *> getChildren() const {
    auto res = replacement ? replacement->getChildren() : doGetChildren();
    return std::vector<const Value *>(res.begin(), res.end());
  }
  /// Physically replaces all instances of a child value.
  /// @param id the id of the value to be replaced
  /// @param newValue the new value
  /// @return number of replacements
  int replaceChild(int id, Value *newValue) {
    return replacement ? replacement->replaceChild(id, newValue)
                       : doReplaceChild(id, newValue);
  }
  /// Physically replaces all instances of a child value.
  /// @param oldValue the old value
  /// @param newValue the new value
  /// @return true if the child was replaced
  int replaceChild(Value *old, Value *newValue) {
    return replaceChild(old->getId(), newValue);
  }

  friend std::ostream &operator<<(std::ostream &os, const Value &a) {
    if (a.replacement)
      return os << a.replacement;
    return a.doFormat(os);
  }

private:
  virtual std::ostream &doFormat(std::ostream &os) const = 0;
  virtual const types::Type *doGetType() const = 0;
  virtual std::vector<Value *> doGetChildren() const = 0;
  virtual int doReplaceChild(int id, Value *newValue) = 0;
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
