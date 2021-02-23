#pragma once

#include "base.h"
#include "types/types.h"

namespace seq {
namespace ir {

class Func;

class Value : public ReplaceableNodeBase<Value>, public IdMixin {
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
    return getActual()->doReplaceUsedValue(id, newValue);
  }
  using IRNode::replaceUsedValue;

  std::vector<types::Type *> getUsedTypes() const final {
    return getActual()->doGetUsedTypes();
  }
  int replaceUsedType(const std::string &name, types::Type *newType) final {
    return getActual()->doReplaceUsedType(name, newType);
  }
  using IRNode::replaceUsedType;

  std::vector<Var *> getUsedVariables() final {
    return getActual()->doGetUsedVariables();
  }
  std::vector<const Var *> getUsedVariables() const final {
    auto ret = getActual()->doGetUsedVariables();
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

  Value *operator==(Value &other);
  Value *operator!=(Value &other);
  Value *operator<(Value &other);
  Value *operator>(Value &other);
  Value *operator<=(Value &other);
  Value *operator>=(Value &other);

  Value *operator+();
  Value *operator-();
  Value *operator~();

  Value *operator+(Value &other);
  Value *operator-(Value &other);
  Value *operator*(Value &other);
  Value *trueDiv(Value &other);
  Value *operator/(Value &other);
  Value *operator%(Value &other);
  Value *pow(Value &other);
  Value *operator<<(Value &other);
  Value *operator>>(Value &other);
  Value *operator&(Value &other);
  Value *operator|(Value &other);
  Value *operator^(Value &other);

  Value *operator||(Value &other);
  Value *operator&&(Value &other);

  template <typename... Args> Value *operator()(Args &&... args) {
    std::vector<Value *> dst;
    stripPack(dst, std::forward<Args>(args)...);
    return doCall(dst);
  }
  Value *operator[](Value &other);

  Value *toInt();
  Value *toBool();
  Value *toStr();

  Value *len();
  Value *iter();

private:
  template <typename... Args>
  static void stripPack(std::vector<Value *> &dst, Value &first, Args &&... args) {
    dst.push_back(&first);
    stripPack(dst, std::forward<Args>(args)...);
  }
  static void stripPack(std::vector<Value *> &dst, Value &first) {
    dst.push_back(&first);
  }
  static void stripPack(std::vector<Value *> &dst) {}

  Value *doUnaryOp(const std::string &name);
  Value *doBinaryOp(const std::string &name, Value &other);

  Value *doCall(const std::vector<Value *> &args);

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
