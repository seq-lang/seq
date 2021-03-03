#pragma once

#include "module.h"
#include "value.h"

namespace seq {
namespace ir {

/// SIR constant base. Once created, constants are immutable.
class Constant : public AcceptorExtend<Constant, Value> {
private:
  /// the type
  types::Type *type;

public:
  static const char NodeId;

  /// Constructs a constant.
  /// @param type the type
  /// @param name the name
  explicit Constant(types::Type *type, std::string name = "")
      : AcceptorExtend(std::move(name)), type(type) {}

private:
  types::Type *doGetType() const override { return type; }

  std::vector<types::Type *> doGetUsedTypes() const override { return {type}; }
  int doReplaceUsedType(const std::string &name, types::Type *newType) override;
};

template <typename ValueType>
class TemplatedConstant
    : public AcceptorExtend<TemplatedConstant<ValueType>, Constant> {
private:
  ValueType val;

public:
  static const char NodeId;

  using AcceptorExtend<TemplatedConstant<ValueType>, Constant>::getModule;
  using AcceptorExtend<TemplatedConstant<ValueType>, Constant>::getSrcInfo;
  using AcceptorExtend<TemplatedConstant<ValueType>, Constant>::getType;

  TemplatedConstant(ValueType v, types::Type *type, std::string name = "")
      : AcceptorExtend<TemplatedConstant<ValueType>, Constant>(type, std::move(name)),
        val(v) {}

  /// @return the internal value.
  ValueType getVal() const { return val; }
  /// Sets the value.
  /// @param v the value
  void setVal(ValueType v) { val = v; }

private:
  std::ostream &doFormat(std::ostream &os) const override {
    fmt::print(os, "{}", val);
    return os;
  }

  Value *doClone() const override {
    return getModule()->template N<TemplatedConstant<ValueType>>(
        getSrcInfo(), val, const_cast<types::Type *>(getType()));
  }
};

using IntConstant = TemplatedConstant<int64_t>;
using FloatConstant = TemplatedConstant<double>;
using BoolConstant = TemplatedConstant<bool>;
using StringConstant = TemplatedConstant<std::string>;

template <typename T> const char TemplatedConstant<T>::NodeId = 0;

template <>
class TemplatedConstant<std::string>
    : public AcceptorExtend<TemplatedConstant<std::string>, Constant> {
private:
  std::string val;

public:
  static const char NodeId;

  TemplatedConstant(std::string v, types::Type *type, std::string name = "")
      : AcceptorExtend(type, std::move(name)), val(std::move(v)) {}

  /// @return the internal value.
  std::string getVal() const { return val; }
  /// Sets the value.
  /// @param v the value
  void setVal(std::string v) { val = std::move(v); }

private:
  std::ostream &doFormat(std::ostream &os) const override {
    fmt::print(os, "\"{}\"", val);
    return os;
  }

  Value *doClone() const override {
    return getModule()->N<TemplatedConstant<std::string>>(
        getSrcInfo(), val, const_cast<types::Type *>(getType()));
  }
};

} // namespace ir
} // namespace seq
