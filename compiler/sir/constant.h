#pragma once

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

  types::Type *getType() const override { return type; }
};

template <typename ValueType>
class TemplatedConstant
    : public AcceptorExtend<TemplatedConstant<ValueType>, Constant> {
private:
  ValueType val;

public:
  static const char NodeId;

  TemplatedConstant(ValueType v, types::Type *type, std::string name = "")
      : AcceptorExtend<TemplatedConstant<ValueType>, Constant>(type, std::move(name)),
        val(v) {}

  /// @return the internal value.
  ValueType getVal() { return val; }

  std::ostream &doFormat(std::ostream &os) const override {
    fmt::print(os, "{}", val);
    return os;
  }
};

using IntConstant = TemplatedConstant<seq_int_t>;
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
  std::string getVal() { return val; }

  std::ostream &doFormat(std::ostream &os) const override {
    fmt::print(os, "\"{}\"", val);
    return os;
  }
};

} // namespace ir
} // namespace seq