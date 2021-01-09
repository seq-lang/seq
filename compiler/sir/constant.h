#pragma once

#include "module.h"
#include "value.h"

namespace seq {
namespace ir {

/// SIR constant base. Once created, constants are immutable.
class Constant : public AcceptorExtend<Constant, Value> {
private:
  /// the type
  const types::Type *type;

public:
  static const char NodeId;

  /// Constructs a constant.
  /// @param type the type
  /// @param name the name
  explicit Constant(const types::Type *type, std::string name = "")
      : AcceptorExtend(std::move(name)), type(type) {}

  const types::Type *getType() const override { return type; }
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

  TemplatedConstant(ValueType v, const types::Type *type, std::string name = "")
      : AcceptorExtend<TemplatedConstant<ValueType>, Constant>(type, std::move(name)),
        val(v) {}

  /// @return the internal value.
  ValueType getVal() { return val; }

private:
  std::ostream &doFormat(std::ostream &os) const override {
    fmt::print(os, "{}", val);
    return os;
  }

  Value *doClone() const override {
    return getModule()->template Nrs<TemplatedConstant<ValueType>>(getSrcInfo(), val,
                                                                   getType());
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

  TemplatedConstant(std::string v, const types::Type *type, std::string name = "")
      : AcceptorExtend(type, std::move(name)), val(std::move(v)) {}

  /// @return the internal value.
  std::string getVal() { return val; }

private:
  std::ostream &doFormat(std::ostream &os) const override {
    fmt::print(os, "\"{}\"", val);
    return os;
  }

  Value *doClone() const override {
    return getModule()->Nrs<TemplatedConstant<std::string>>(getSrcInfo(), val,
                                                            getType());
  }
};

enum IntrinsicType { NEXT, DONE };

std::ostream &operator<<(std::ostream &os, const IntrinsicType &t);

using IntrinsicConstant = TemplatedConstant<IntrinsicType>;

template <>
class TemplatedConstant<IntrinsicType>
    : public AcceptorExtend<TemplatedConstant<IntrinsicType>, Constant> {
private:
  IntrinsicType val;
  const types::Type *returnType;

public:
  static const char NodeId;

  TemplatedConstant(IntrinsicType v, const types::Type *returnType,
                    std::string name = "")
      : AcceptorExtend(nullptr, std::move(name)), val(v), returnType(returnType) {}

  /// @return the internal value.
  IntrinsicType getVal() { return val; }

  const types::Type *getReturnType() const { return returnType; }

private:
  std::ostream &doFormat(std::ostream &os) const override {
    fmt::print(os, "{}", val);
    return os;
  }

  Value *doClone() const override {
    return getModule()->Nrs<TemplatedConstant<IntrinsicType>>(getSrcInfo(), val,
                                                              getType());
  }
};

/// Signifies an undefined value.
class UndefinedConstant : public AcceptorExtend<UndefinedConstant, Constant> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

private:
  std::ostream &doFormat(std::ostream &os) const override { return os << "undef"; }

  Value *doClone() const override {
    return getModule()->Nrs<UndefinedConstant>(getSrcInfo(), getType());
  }
};

} // namespace ir
} // namespace seq
