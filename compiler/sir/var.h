#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "types/types.h"
#include "util/common.h"
#include "value.h"

namespace seq {
namespace ir {

class Func;

/// SIR object representing a variable.
class Var : public AcceptorExtend<Var, IRNode>, public IdMixin, public ParentFuncMixin {
private:
  /// the variable's type
  const types::Type *type;
  /// true if the variable is global
  bool global;

public:
  static const char NodeId;

  /// Constructs a variable.
  /// @param type the variable's type
  /// @param global true if the variable is global
  /// @param name the variable's name
  explicit Var(const types::Type *type, bool global = false, std::string name = "")
      : AcceptorExtend(std::move(name)), type(type), global(global) {}

  /// @return the type
  const types::Type *getType() const { return type; }
  /// Sets the type.
  /// @param t the new type
  void setType(const types::Type *t) { type = t; }

  /// @return true if the variable is global
  bool isGlobal() const { return global; }
  /// Sets the global flag.
  /// @param v the new value
  void setGlobal(bool v = true) { global = v; }

  std::string referenceString() const override {
    return fmt::format(FMT_STRING("{}.{}"), getName(), getId());
  }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Value that contains an unowned variable reference.
class VarValue : public AcceptorExtend<VarValue, Value> {
private:
  /// the referenced var
  Var *val;

public:
  static const char NodeId;

  /// Constructs a variable value.
  /// @param val the referenced value
  /// @param name the name
  explicit VarValue(Var *val, std::string name = "")
      : AcceptorExtend(std::move(name)), val(val) {}

  const types::Type *getType() const override { return val->getType(); }

  /// @return the variable
  Var *getVar() { return val; }
  /// @return the variable
  const Var *getVar() const { return val; }
  /// Sets the variable.
  /// @param v the new variable
  void setVar(Var *v) { val = v; }

private:
  std::ostream &doFormat(std::ostream &os) const override {
    return os << val->referenceString();
  }

  Value *doClone() const override;
};

/// Value that represents a pointer.
class PointerValue : public AcceptorExtend<PointerValue, Value> {
private:
  /// the referenced var
  Var *val;

public:
  static const char NodeId;

  /// Constructs a variable value.
  /// @param val the referenced value
  /// @param name the name
  explicit PointerValue(Var *val, std::string name = "")
      : AcceptorExtend(std::move(name)), val(val) {}

  types::Type *getType() const override;

  /// @return the variable
  Var *getVar() { return val; }
  /// @return the variable
  const Var *getVar() const { return val; }
  /// Sets the variable.
  /// @param v the new variable
  void setVar(Var *v) { val = v; }

private:
  std::ostream &doFormat(std::ostream &os) const override {
    return os << '&' << val->referenceString();
  }

  Value *doClone() const override;
};

} // namespace ir
} // namespace seq

// See https://github.com/fmtlib/fmt/issues/1283.
namespace fmt {
using seq::ir::Var;

template <typename Char>
struct formatter<Var, Char> : fmt::v6::internal::fallback_formatter<Var, Char> {};
} // namespace fmt
