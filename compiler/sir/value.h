#pragma once

#include "base.h"
#include "types/types.h"

namespace seq {
namespace ir {

class Func;

class Value : public ReplaceableNodeBase<Value>,
              public IdMixin,
              public ParentFuncMixin {
public:
  static const char NodeId;

  /// Constructs a value.
  /// @param the value's name
  explicit Value(std::string name = "") : ReplaceableNodeBase(std::move(name)) {}

  virtual ~Value() noexcept = default;

  std::string referenceString() const final {
    return fmt::format(FMT_STRING("{}.{}"), getActual()->getName(),
                       getActual()->getId());
  }

  std::vector<Value *> getUsedValues() final { return getActual()->doGetUsedValues(); }
  std::vector<const Value *> getUsedValues() const final {
    auto ret = getActual()->doGetUsedValues();
    return std::vector<const Value *>(ret.begin(), ret.end());
  }
  int replaceUsedValue(int id, Value *newValue) final {
    return doReplaceUsedValue(id, newValue);
  }
  using IRNode::replaceUsedValue;

  std::vector<types::Type *> getUsedTypes() final {
    return getActual()->doGetUsedTypes();
  }
  std::vector<const types::Type *> getUsedTypes() const final {
    auto ret = getActual()->doGetUsedTypes();
    return std::vector<const types::Type *>(ret.begin(), ret.end());
  }
  int replaceUsedType(const std::string &name, types::Type *newType) final {
    return getActual()->doReplaceUsedType(name, newType);
  }
  using IRNode::replaceUsedType;

  std::vector<Var *> getUsedVariables() final { return doGetUsedVariables(); }
  std::vector<const Var *> getUsedVariables() const final {
    auto ret = doGetUsedVariables();
    return std::vector<const Var *>(ret.begin(), ret.end());
  }
  int replaceUsedVariable(int id, Var *newVar) final {
    return getActual()->doReplaceUsedVariable(id, newVar);
  }
  using IRNode::replaceUsedVariable;

  /// @return the value's type
  types::Type *getType() { return const_cast<types::Type *>(getActual()->doGetType()); }
  /// @return the value's type
  const types::Type *getType() const { return getActual()->doGetType(); }

  /// @return a clone of the value
  Value *clone() const;

private:
  virtual const types::Type *doGetType() const = 0;

  virtual std::vector<Value *> doGetUsedValues() const { return {}; }
  virtual int doReplaceUsedValue(int id, Value *newValue) { return 0; }

  virtual std::vector<types::Type *> doGetUsedTypes() const { return {}; }
  virtual int doReplaceUsedType(const std::string &name, types::Type *newType) {
    return 0;
  }

  virtual std::vector<Var *> doGetUsedVariables() const { return {}; }
  virtual int doReplaceUsedVariable(int id, Var *newVar) { return 0; }

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
