#pragma once

#include <iterator>
#include <memory>
#include <string>
#include <unordered_map>

#include "util/fmt/format.h"
#include "util/fmt/ostream.h"

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
  std::unique_ptr<Func> mainFunc;
  /// the module's argv variable
  std::unique_ptr<Var> argVar;
  /// the global variables list
  std::list<std::unique_ptr<Var>> vars;
  /// the global variables map
  std::unordered_map<int, std::list<std::unique_ptr<Var>>::iterator> varMap;
  /// the global value list
  std::list<std::unique_ptr<Value>> values;
  /// the global value map
  std::unordered_map<int, std::list<std::unique_ptr<Value>>::iterator> valueMap;
  /// the global types list
  std::list<std::unique_ptr<types::Type>> types;
  /// the global types map
  std::unordered_map<std::string, std::list<std::unique_ptr<types::Type>>::iterator>
      typesMap;

public:
  static const char NodeId;

  /// Constructs an SIR module.
  /// @param name the module name
  explicit IRModule(std::string name);

  /// @return the main function
  Func *getMainFunc() { return mainFunc.get(); }
  /// @return the main function
  const Func *getMainFunc() const { return mainFunc.get(); }

  /// @return the arg var
  Var *getArgVar() { return argVar.get(); }
  /// @return the arg var
  const Var *getArgVar() const { return argVar.get(); }

  /// @return iterator to the first symbol
  auto begin() { return util::raw_ptr_adaptor(vars.begin()); }
  /// @return iterator beyond the last symbol
  auto end() { return util::raw_ptr_adaptor(vars.end()); }
  /// @return iterator to the first symbol
  auto begin() const { return util::const_raw_ptr_adaptor(vars.begin()); }
  /// @return iterator beyond the last symbol
  auto end() const { return util::const_raw_ptr_adaptor(vars.end()); }
  /// @return a pointer to the first symbol
  Var *front() { return vars.front().get(); }
  /// @return a pointer to the last symbol
  Var *back() { return vars.back().get(); }
  /// @return a pointer to the first symbol
  const Var *front() const { return vars.front().get(); }
  /// @return a pointer to the last symbol
  const Var *back() const { return vars.back().get(); }

  /// Removes a given var.
  /// @param v the var
  void remove(const Var *v) {
    auto it = varMap.find(v->getId());
    vars.erase(it->second);
    varMap.erase(it);
  }

  /// @return iterator to the first value
  auto values_begin() { return util::raw_ptr_adaptor(values.begin()); }
  /// @return iterator beyond the last value
  auto values_end() { return util::raw_ptr_adaptor(values.end()); }
  /// @return iterator to the first value
  auto values_begin() const { return util::const_raw_ptr_adaptor(values.begin()); }
  /// @return iterator beyond the last value
  auto values_end() const { return util::const_raw_ptr_adaptor(values.end()); }
  /// @return a pointer to the first value
  Value *values_front() { return values.front().get(); }
  /// @return a pointer to the last value
  Value *values_back() { return values.back().get(); }
  /// @return a pointer to the first value
  const Value *values_front() const { return values.front().get(); }
  /// @return a pointer to the last value
  const Value *values_back() const { return values.back().get(); }

  /// Removes a given value.
  /// @param v the value
  void remove(const Value *v) {
    auto it = valueMap.find(v->getId());
    values.erase(it->second);
    valueMap.erase(it);
  }

  /// @return iterator to the first type
  auto types_begin() { return util::raw_ptr_adaptor(types.begin()); }
  /// @return iterator beyond the last type
  auto types_end() { return util::raw_ptr_adaptor(types.end()); }
  /// @return iterator to the first type
  auto types_begin() const { return util::const_raw_ptr_adaptor(types.begin()); }
  /// @return iterator beyond the last type
  auto types_end() const { return util::const_raw_ptr_adaptor(types.end()); }
  /// @return a pointer to the first type
  types::Type *types_front() { return types.front().get(); }
  /// @return a pointer to the last type
  types::Type *types_back() { return types.back().get(); }
  /// @return a pointer to the first type
  const types::Type *types_front() const { return types.front().get(); }
  /// @return a pointer to the last type
  const types::Type *types_back() const { return types.back().get(); }

  /// @param name the type's name
  /// @return the type with the given name
  types::Type *getType(const std::string &name) {
    auto it = typesMap.find(name);
    return it == typesMap.end() ? nullptr : it->second->get();
  }

  /// @param name the type's name
  /// @return the type with the given name
  const types::Type *getType(const std::string &name) const {
    auto it = typesMap.find(name);
    return it == typesMap.end() ? nullptr : it->second->get();
  }

  /// Removes a given type.
  /// @param t the type
  void remove(const types::Type *t) {
    auto it = typesMap.find(t->getName());
    types.erase(it->second);
    typesMap.erase(it);
  }

  /// Finds a function by mangled name and argument types.
  /// @param name the mangled name
  /// @param argTypes the argument types
  /// @return the function if it exists, nullptr otherwise
  Func *lookupFunc(const std::string &name, const std::vector<types::Type *> &argTypes);
  /// Finds a function by mangled name.
  /// @param name the mangled name
  /// @param argTypes the argument types
  /// @return the function if it exists, nullptr otherwise
  Func *lookupFunc(const std::string &name);
  /// Finds a builtin function by unmangled name.
  /// @param name the unmangled name
  /// @return the function if it exists, nullptr otherwise
  BodiedFunc *lookupBuiltinFunc(const std::string &name);

  template <typename DesiredType, typename... Args>
  DesiredType *N(seq::SrcInfo s, Args &&... args) {
    auto *ret = new DesiredType(std::forward<Args>(args)...);
    ret->setModule(this);
    ret->setSrcInfo(s);

    store(ret);
    return ret;
  }

  template <typename DesiredType, typename... Args>
  DesiredType *N(const seq::SrcObject *s, Args &&... args) {
    return N<DesiredType>(s->getSrcInfo(), std::forward<Args>(args)...);
  }

  template <typename DesiredType, typename... Args> DesiredType *Nr(Args &&... args) {
    return N<DesiredType>(seq::SrcInfo(), std::forward<Args>(args)...);
  }

  types::Type *getPointerType(types::Type *base) {
    auto name = types::PointerType::getInstanceName(base);
    if (auto *rVal = getType(name))
      return rVal;
    return Nr<types::PointerType>(base);
  }

  types::Type *getArrayType(types::Type *base) {
    auto name = types::ArrayType::getInstanceName(base);
    if (auto *rVal = getType(name))
      return rVal;
    return Nr<types::ArrayType>(getPointerType(base), getIntType());
  }

  types::Type *getGeneratorType(types::Type *base) {
    auto name = types::GeneratorType::getInstanceName(base);
    if (auto *rVal = getType(name))
      return rVal;
    return Nr<types::GeneratorType>(base);
  }

  types::Type *getOptionalType(types::Type *base) {
    auto name = types::OptionalType::getInstanceName(base);
    if (auto *rVal = getType(name))
      return rVal;
    return Nr<types::OptionalType>(base);
  }

  types::Type *getVoidType() {
    if (auto *rVal = getType(VOID_NAME))
      return rVal;
    return Nr<types::VoidType>();
  }

  types::Type *getBoolType() {
    if (auto *rVal = getType(BOOL_NAME))
      return rVal;
    return Nr<types::BoolType>();
  }

  types::Type *getByteType() {
    if (auto *rVal = getType(BYTE_NAME))
      return rVal;
    return Nr<types::ByteType>();
  }

  types::Type *getIntType() {
    if (auto *rVal = getType(INT_NAME))
      return rVal;
    return Nr<types::IntType>();
  }

  types::Type *getFloatType() {
    if (auto *rVal = getType(FLOAT_NAME))
      return rVal;
    return Nr<types::FloatType>();
  }

  types::Type *getStringType() {
    if (auto *rVal = getType(STRING_NAME))
      return rVal;
    return Nr<types::RecordType>(
        STRING_NAME,
        std::vector<types::Type *>{getIntType(), getPointerType(getByteType())},
        std::vector<std::string>{"len", "ptr"});
  }

  types::Type *getFuncType(types::Type *rType, std::vector<types::Type *> argTypes) {
    auto name = types::FuncType::getInstanceName(rType, argTypes);
    if (auto *rVal = getType(name))
      return rVal;
    return Nr<types::FuncType>(rType, std::move(argTypes));
  }

  types::Type *getVoidRetAndArgFuncType() { return getFuncType(getVoidType(), {}); }

  types::Type *getMemberedType(const std::string &name, bool ref = false) {
    auto *rVal = getType(name);

    if (!rVal) {
      if (ref) {
        auto contentName = name + ".contents";
        auto *record = getType(contentName);
        if (!record) {
          record = Nr<types::RecordType>(contentName);
        }
        rVal = Nr<types::RefType>(name, record->as<types::RecordType>());
      } else {
        rVal = Nr<types::RecordType>(name);
      }
    }

    return rVal;
  }

  types::Type *getIntNType(unsigned len, bool sign) {
    auto name = types::IntNType::getInstanceName(len, sign);
    if (auto *rVal = getType(name))
      return rVal;
    return Nr<types::IntNType>(len, sign);
  }

private:
  void store(types::Type *t) {
    types.emplace_back(t);
    typesMap[t->getName()] = std::prev(types.end());
  }
  void store(Value *v) {
    values.emplace_back(v);
    valueMap[v->getId()] = std::prev(values.end());
  }
  void store(Var *v) {
    vars.emplace_back(v);
    varMap[v->getId()] = std::prev(vars.end());
  }

  std::ostream &doFormat(std::ostream &os) const override;
};

} // namespace ir
} // namespace seq
