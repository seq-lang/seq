#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "util/fmt/format.h"
#include "util/fmt/ostream.h"

#include "types/types.h"
#include "util/common.h"
#include "value.h"

namespace seq {
namespace ir {

class Func;
class Var;

/// SIR object representing a variable.
class Var : public ReplaceableNodeBase<Var>, public IdMixin {
private:
  /// the variable's type
  types::Type *type;
  /// true if the variable is global
  bool global;

public:
  static const char NodeId;

  /// Constructs a variable.
  /// @param type the variable's type
  /// @param global true if the variable is global
  /// @param name the variable's name
  explicit Var(types::Type *type, bool global = false, std::string name = "")
      : ReplaceableNodeBase(std::move(name)), type(type), global(global) {}

  virtual ~Var() noexcept = default;

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

  /// @return the type
  types::Type *getType() { return getActual()->type; }
  /// @return the type
  const types::Type *getType() const { return getActual()->type; }
  /// Sets the type.
  /// @param t the new type
  void setType(types::Type *t) { getActual()->type = t; }

  /// @return true if the variable is global
  bool isGlobal() const { return getActual()->global; }
  /// Sets the global flag.
  /// @param v the new value
  void setGlobal(bool v = true) { getActual()->global = v; }

  std::string referenceString() const final {
    return fmt::format(FMT_STRING("{}.{}"), getActual()->getName(),
                       getActual()->getId());
  }

  /// @return a clone of the value
  Var *clone() const;

private:
  virtual Var *doClone() const;

  std::ostream &doFormat(std::ostream &os) const override;

protected:
  virtual std::vector<Value *> doGetUsedValues() const { return {}; }
  virtual int doReplaceUsedValue(int id, Value *newValue) { return 0; }

  virtual std::vector<types::Type *> doGetUsedTypes() const { return {type}; }
  virtual int doReplaceUsedType(const std::string &name, types::Type *newType);

  virtual std::vector<Var *> doGetUsedVariables() const { return {}; }
  virtual int doReplaceUsedVariable(int id, Var *newVar) { return 0; }
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

  const types::Type *doGetType() const override { return val->getType(); }

  std::vector<Var *> doGetUsedVariables() const override { return {val}; }
  int doReplaceUsedVariable(int id, Var *newVar) override;

  Value *doClone() const override;
};

/// Value that represents a pointer.
class PointerValue : public AcceptorExtend<PointerValue, Value> {
private:
  /// the referenced var
  Var *val;
  /// the pointer type
  types::Type *pointerType;

public:
  static const char NodeId;

  /// Constructs a variable value.
  /// @param val the referenced value
  /// @param name the name
  explicit PointerValue(Var *val, types::Type *pointerType, std::string name = "")
      : AcceptorExtend(std::move(name)), val(val), pointerType(pointerType) {}

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

  const types::Type *doGetType() const override { return pointerType; }

  std::vector<Var *> doGetUsedVariables() const override { return {val}; }
  int doReplaceUsedVariable(int id, Var *newVar) override;

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
