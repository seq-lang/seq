#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "func.h"
#include "util/iterators.h"
#include "value.h"
#include "var.h"

namespace seq {
namespace ir {

/// SIR object representing a program.
class IRModule : public AcceptorExtend<IRModule, IRNode> {
public:
  static const std::string VOID_NAME;
  static const std::string BOOL_NAME;
  static const std::string BYTE_NAME;
  static const std::string INT_NAME;
  static const std::string FLOAT_NAME;
  static const std::string STRING_NAME;

private:
  /// the module's "main" function
  FuncPtr mainFunc;
  /// the module's argv variable
  VarPtr argVar;
  /// the global symbols table
  std::list<VarPtr> symbols;
  /// the global types table
  std::unordered_map<std::string, types::TypePtr> types;

public:
  static const char NodeId;

  /// Constructs an SIR module.
  /// @param name the module name
  explicit IRModule(std::string name) : AcceptorExtend(std::move(name)) {}

  /// @return the main function
  Func *getMainFunc() { return mainFunc.get(); }
  /// @return the main function
  const Func *getMainFunc() const { return mainFunc.get(); }
  /// Sets the main function.
  /// @param f the new funciton
  void setMainFunc(FuncPtr f) { mainFunc = std::move(f); }

  /// @return the arg var
  Var *getArgVar() { return argVar.get(); }
  /// @return the arg var
  const Var *getArgVar() const { return argVar.get(); }
  /// Sets the arg var.
  /// @param f the new function
  void setArgVar(VarPtr f) { argVar = std::move(f); }

  /// @return iterator to the first symbol
  auto begin() { return util::raw_ptr_adaptor(symbols.begin()); }
  /// @return iterator beyond the last symbol
  auto end() { return util::raw_ptr_adaptor(symbols.end()); }
  /// @return iterator to the first symbol
  auto begin() const { return util::const_raw_ptr_adaptor(symbols.begin()); }
  /// @return iterator beyond the last symbol
  auto end() const { return util::const_raw_ptr_adaptor(symbols.end()); }

  /// @return a pointer to the first symbol
  Var *front() { return symbols.front().get(); }
  /// @return a pointer to the last symbol
  Var *back() { return symbols.back().get(); }
  /// @return a pointer to the first symbol
  const Var *front() const { return symbols.front().get(); }
  /// @return a pointer to the last symbol
  const Var *back() const { return symbols.back().get(); }

  /// Inserts an symbol at the given position.
  /// @param pos the position
  /// @param v the symbol
  /// @return an iterator to the newly added symbol
  template <typename It> auto insert(It pos, VarPtr v) {
    return util::raw_ptr_adaptor(symbols.insert(pos.internal, std::move(v)));
  }
  /// Appends an symbol.
  /// @param v the new symbol
  void push_back(VarPtr v) { symbols.push_back(std::move(v)); }

  /// Erases the symbol at the given position.
  /// @param pos the position
  /// @return iterator following the removed symbol.
  template <typename It> auto erase(It pos) {
    return util::raw_ptr_adaptor(symbols.erase(pos.internal));
  }

  /// @param name the type's name
  /// @return the type with the given name
  types::Type *getType(const std::string &name) {
    auto it = types.find(name);
    return it == types.end() ? nullptr : it->second.get();
  }

  template <typename DesiredType, typename... Args>
  DesiredType *Nrs(const seq::SrcObject *s, Args &&... args) {
    auto *ret = new DesiredType(std::forward<Args>(args)...);
    ret->setModule(this);
    ret->setSrcInfo(s->getSrcInfo());
    return ret;
  }

  template <typename DesiredType, typename... Args>
  DesiredType *Nrs(seq::SrcInfo s, Args &&... args) {
    auto *ret = new DesiredType(std::forward<Args>(args)...);
    ret->setModule(this);
    ret->setSrcInfo(s);
    return ret;
  }

  template <typename DesiredType, typename... Args> DesiredType *Nr(Args &&... args) {
    auto *ret = new DesiredType(std::forward<Args>(args)...);
    ret->setModule(this);
    return ret;
  }

  template <typename DesiredType, typename... Args>
  std::unique_ptr<DesiredType> Nxs(const seq::SrcObject *s, Args &&... args) {
    auto ret = std::make_unique<DesiredType>(std::forward<Args>(args)...);
    ret->setModule(this);
    ret->setSrcInfo(s->getSrcInfo());
    return std::move(ret);
  }

  template <typename DesiredType, typename... Args>
  std::unique_ptr<DesiredType> Nx(Args &&... args) {
    auto ret = std::make_unique<DesiredType>(std::forward<Args>(args)...);
    ret->setModule(this);
    return std::move(ret);
  }

  types::Type *getPointerType(const types::Type *base) {
    auto name = types::PointerType::getName(base);
    auto *rVal = getType(name);
    if (!rVal) {
      rVal = Nr<types::PointerType>(base);
      types[name] = types::TypePtr(rVal);
    }
    return rVal;
  }

  types::Type *getArrayType(const types::Type *base) {
    auto name = types::ArrayType::getName(base);
    auto *rVal = getType(name);
    if (!rVal) {
      rVal = Nr<types::ArrayType>(getPointerType(base), getIntType());
      types[name] = types::TypePtr(rVal);
    }
    return rVal;
  }

  types::Type *getGeneratorType(const types::Type *base) {
    auto name = types::GeneratorType::getName(base);
    auto *rVal = getType(name);
    if (!rVal) {
      rVal = Nr<types::GeneratorType>(base);
      types[name] = types::TypePtr(rVal);
    }
    return rVal;
  }

  types::Type *getOptionalType(const types::Type *base) {
    auto name = types::OptionalType::getName(base);
    auto *rVal = getType(name);
    if (!rVal) {
      rVal = Nr<types::OptionalType>(base);
      types[name] = types::TypePtr(rVal);
    }
    return rVal;
  }

  types::Type *getVoidType() {
    auto *rVal = getType(VOID_NAME);
    if (!rVal) {
      rVal = Nr<types::VoidType>();
      types[VOID_NAME] = types::TypePtr(rVal);
    }
    return rVal;
  }

  types::Type *getBoolType() {
    auto *rVal = getType(BOOL_NAME);
    if (!rVal) {
      rVal = Nr<types::BoolType>();
      types[BOOL_NAME] = types::TypePtr(rVal);
    }
    return rVal;
  }

  types::Type *getByteType() {
    auto *rVal = getType(BYTE_NAME);
    if (!rVal) {
      rVal = Nr<types::ByteType>();
      types[BYTE_NAME] = types::TypePtr(rVal);
    }
    return rVal;
  }

  types::Type *getIntType() {
    auto *rVal = getType(INT_NAME);
    if (!rVal) {
      rVal = Nr<types::IntType>();
      types[INT_NAME] = types::TypePtr(rVal);
    }
    return rVal;
  }

  types::Type *getFloatType() {
    auto *rVal = getType(FLOAT_NAME);
    if (!rVal) {
      rVal = Nr<types::FloatType>();
      types[FLOAT_NAME] = types::TypePtr(rVal);
    }
    return rVal;
  }

  types::Type *getStringType() {
    auto *rVal = getType(STRING_NAME);
    if (!rVal) {
      rVal = new types::RecordType(
          STRING_NAME, {getIntType(), getPointerType(getByteType())}, {"len", "ptr"});
      types[STRING_NAME] = types::TypePtr(rVal);
    }
    return rVal;
  }

  types::Type *getFuncType(const types::Type *rType,
                           std::vector<const types::Type *> argTypes) {
    auto name = types::FuncType::getName(rType, argTypes);
    auto *rVal = getType(name);
    if (!rVal) {
      rVal = Nr<types::FuncType>(rType, std::move(argTypes));
      types[name] = types::TypePtr(rVal);
    }
    return rVal;
  }

  types::Type *getVoidRetAndArgFuncType() { return getFuncType(getVoidType(), {}); }

  types::Type *getMemberedType(std::string name, bool ref = false) {
    auto *rVal = getType(name);

    if (!rVal) {
      if (ref) {
        auto contentName = name + ".contents";
        auto *record = getType(contentName);
        if (!record) {
          record = Nr<types::RecordType>(contentName);
          types[contentName] = types::TypePtr(record);
        }
        rVal = Nr<types::RefType>(name, record->as<types::RecordType>());
      } else {
        rVal = Nr<types::RecordType>(name);
      }
      types[name] = types::TypePtr(rVal);
    }

    return rVal;
  }

  types::Type *getIntNType(unsigned len, bool sign) {
    auto name = types::IntNType::getName(len, sign);
    auto *rVal = getType(name);
    if (!rVal) {
      rVal = Nr<types::IntNType>(len, sign);
      types[name] = types::TypePtr(rVal);
    }
    return rVal;
  }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

using IRModulePtr = std::unique_ptr<IRModule>;

} // namespace ir
} // namespace seq
