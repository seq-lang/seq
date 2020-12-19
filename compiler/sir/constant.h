#pragma once

#include "value.h"

namespace seq {
namespace ir {

/// SIR constant base. Once created, constants are immutable.
class Constant : public Value {
private:
  /// the type
  types::Type *type;

public:
  /// Constructs a constant.
  /// @param type the type
  /// @param name the name
  explicit Constant(types::Type *type, std::string name = "")
      : Value(std::move(name)), type(type) {}

  types::Type *getType() const override { return type; }
};

template <typename ValueType> class TemplatedConstant : public Constant {
private:
  ValueType val;

public:
  TemplatedConstant(ValueType v, types::Type *type, std::string name = "")
      : Constant(type, std::move(name)), val(v) {}

  void accept(util::SIRVisitor &v) override { v.visit(this); }

  /// @return the internal value.
  ValueType getVal() { return val; }

  std::ostream &doFormat(std::ostream &os) const override { return os << val; }
};

using IntConstant = TemplatedConstant<seq_int_t>;
using FloatConstant = TemplatedConstant<float>;
using BoolConstant = TemplatedConstant<bool>;
using StringConstant = TemplatedConstant<std::string>;

template <> class TemplatedConstant<std::string> : public Constant {
private:
  std::string val;

public:
  TemplatedConstant(std::string v, types::Type *type, std::string name = "")
      : Constant(type, std::move(name)), val(std::move(v)) {}

  void accept(util::SIRVisitor &v) override { v.visit(this); }

  /// @return the internal value.
  std::string getVal() { return val; }

  std::ostream &doFormat(std::ostream &os) const override {
    return os << '"' << val << '"';
  }
};

} // namespace ir
} // namespace seq