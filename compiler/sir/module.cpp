#include "module.h"

#include <algorithm>
#include <memory>

#include "parser/cache.h"

#include "func.h"

namespace {

using namespace seq::ir;

std::vector<seq::ast::types::TypePtr>
translateGenerics(std::vector<types::Generic> &generics) {
  std::vector<seq::ast::types::TypePtr> ret;
  for (auto &g : generics) {
    ret.push_back(std::make_shared<seq::ast::types::LinkType>(
        g.isStatic() ? std::make_shared<seq::ast::types::StaticType>(g.getStaticValue())
                     : g.getTypeValue()->getAstType()));
  }
  return ret;
}

std::vector<std::pair<std::string, seq::ast::types::TypePtr>>
generateDummyNames(std::vector<types::Type *> &types) {
  std::vector<std::pair<std::string, seq::ast::types::TypePtr>> ret;
  for (auto *t : types) {
    ret.emplace_back("", t->getAstType());
  }
  return ret;
}

std::vector<seq::ast::types::TypePtr> translateArgs(types::Type *rType,
                                                    std::vector<types::Type *> &types) {
  std::vector<seq::ast::types::TypePtr> ret = {rType->getAstType()};
  for (auto *t : types) {
    ret.push_back(t->getAstType());
  }
  return ret;
}

} // namespace

namespace seq {
namespace ir {

const std::string IRModule::VOID_NAME = "void";
const std::string IRModule::BOOL_NAME = "bool";
const std::string IRModule::BYTE_NAME = "byte";
const std::string IRModule::INT_NAME = "int";
const std::string IRModule::FLOAT_NAME = "float";
const std::string IRModule::STRING_NAME = "str";

const char IRModule::NodeId = 0;

IRModule::IRModule(std::string name, std::shared_ptr<ast::Cache> cache)
    : AcceptorExtend(std::move(name)), cache(std::move(cache)) {
  mainFunc = std::make_unique<BodiedFunc>(getVoidRetAndArgFuncType(), "main");
  mainFunc->setModule(this);
  mainFunc->setReplaceable(false);
  argVar = std::make_unique<Var>(getArrayType(getStringType()), true, "argv");
  argVar->setModule(this);
  argVar->setReplaceable(false);
}

types::Type *IRModule::getPointerType(types::Type *base) {
  auto name = types::PointerType::getInstanceName(base);
  if (auto *rVal = getType(name))
    return rVal;
  return Nr<types::PointerType>(base);
}

types::Type *IRModule::getArrayType(types::Type *base) {
  auto name = types::ArrayType::getInstanceName(base);
  if (auto *rVal = getType(name))
    return rVal;
  return Nr<types::ArrayType>(getPointerType(base), getIntType());
}

types::Type *IRModule::getGeneratorType(types::Type *base) {
  auto name = types::GeneratorType::getInstanceName(base);
  if (auto *rVal = getType(name))
    return rVal;
  return Nr<types::GeneratorType>(base);
}

types::Type *IRModule::getOptionalType(types::Type *base) {
  auto name = types::OptionalType::getInstanceName(base);
  if (auto *rVal = getType(name))
    return rVal;
  return Nr<types::OptionalType>(base);
}

types::Type *IRModule::getVoidType() {
  if (auto *rVal = getType(VOID_NAME))
    return rVal;
  return Nr<types::VoidType>();
}

types::Type *IRModule::getBoolType() {
  if (auto *rVal = getType(BOOL_NAME))
    return rVal;
  return Nr<types::BoolType>();
}

types::Type *IRModule::getByteType() {
  if (auto *rVal = getType(BYTE_NAME))
    return rVal;
  return Nr<types::ByteType>();
}

types::Type *IRModule::getIntType() {
  if (auto *rVal = getType(INT_NAME))
    return rVal;
  return Nr<types::IntType>();
}

types::Type *IRModule::getFloatType() {
  if (auto *rVal = getType(FLOAT_NAME))
    return rVal;
  return Nr<types::FloatType>();
}

types::Type *IRModule::getStringType() {
  if (auto *rVal = getType(STRING_NAME))
    return rVal;
  return Nr<types::RecordType>(
      STRING_NAME,
      std::vector<types::Type *>{getIntType(), getPointerType(getByteType())},
      std::vector<std::string>{"len", "ptr"});
}

types::Type *IRModule::getFuncType(const std::string &name, types::Type *rType,
                                   std::vector<types::Type *> argTypes) {
  if (auto *rVal = getType(name))
    return rVal;
  return Nr<types::FuncType>(name, rType, std::move(argTypes));
}

types::Type *IRModule::getVoidRetAndArgFuncType() {
  return getFuncType("internal.void[void]", getVoidType(), {});
}

types::Type *IRModule::getMemberedType(const std::string &name, bool ref) {
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

types::Type *IRModule::getIntNType(unsigned int len, bool sign) {
  auto name = types::IntNType::getInstanceName(len, sign);
  if (auto *rVal = getType(name))
    return rVal;
  return Nr<types::IntNType>(len, sign);
}

Func *IRModule::getOrRealizeMethod(types::Type *parent, const std::string &methodName,
                                   types::Type *rType, std::vector<types::Type *> args,
                                   std::vector<types::Generic> generics) {

  auto method = cache->findMethod(parent->getAstType()->getClass().get(), methodName,
                                  generateDummyNames(args));
  return cache->realizeFunction(method, translateArgs(rType, args),
                                translateGenerics(generics));
}

Func *IRModule::getOrRealizeFunc(const std::string &funcName, types::Type *rType,
                                 std::vector<types::Type *> args,
                                 std::vector<types::Generic> generics) {
  auto func = cache->findFunction(funcName);
  return cache->realizeFunction(func, translateArgs(rType, args),
                                translateGenerics(generics));
}

types::Type *IRModule::getOrRealizeType(const std::string &typeName,
                                        std::vector<types::Generic> generics) {
  auto type = cache->findClass(typeName);
  return cache->realizeType(type, translateGenerics(generics));
}

std::ostream &IRModule::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("module {}{{\n"), referenceString());
  fmt::print(os, "{}\n", *mainFunc);

  for (auto &g : vars) {
    if (g->isGlobal())
      fmt::print(os, FMT_STRING("{}\n"), *g);
  }
  os << '}';
  return os;
}

} // namespace ir
} // namespace seq
