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
  std::unique_ptr<Func> mainFunc;
  /// the module's argv variabl{}e
  std::unique_ptr<Var> argVar;
  /// the global variables list
  std::list<std::unique_ptr<Var>> vars;
  /// the global value list
  std::list<std::unique_ptr<Value>> values;
  /// the global types table
  std::unordered_map<std::string, std::unique_ptr<types::Type>> types;

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

  /// @param name the type's name
  /// @return the type with the given name
  types::Type *getType(const std::string &name) {
    auto it = types.find(name);
    return it == types.end() ? nullptr : it->second.get();
  }

  types::Type *getPointerType(const types::Type *base) {
    auto name = types::PointerType::getInstanceName(base);
    if (auto *rVal = getType(name))
      return rVal;
    return Nr<types::PointerType>(base);
  }

  types::Type *getArrayType(const types::Type *base) {
    auto name = types::ArrayType::getInstanceName(base);
    if (auto *rVal = getType(name))
      return rVal;
    return Nr<types::ArrayType>(getPointerType(base), getIntType());
  }

  types::Type *getGeneratorType(const types::Type *base) {
    auto name = types::GeneratorType::getInstanceName(base);
    if (auto *rVal = getType(name))
      return rVal;
    return Nr<types::GeneratorType>(base);
  }

  types::Type *getOptionalType(const types::Type *base) {
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
        std::vector<const types::Type *>{getIntType(), getPointerType(getByteType())},
        std::vector<std::string>{"len", "ptr"});
  }

  types::Type *getFuncType(const types::Type *rType,
                           std::vector<const types::Type *> argTypes) {
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
  void store(types::Type *t) { types[t->getName()] = std::unique_ptr<types::Type>(t); }
  void store(Value *v) { values.emplace_back(v); }
  void store(Var *v) { vars.emplace_back(v); }

  std::ostream &doFormat(std::ostream &os) const override;
};

} // namespace ir
} // namespace seq
