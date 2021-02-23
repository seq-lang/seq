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
    assert(g.isStatic() || g.getTypeValue());
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
    assert(t->getAstType());
    ret.emplace_back("", t->getAstType());
  }
  return ret;
}

std::vector<seq::ast::types::TypePtr> translateArgs(std::vector<types::Type *> &types) {
  std::vector<seq::ast::types::TypePtr> ret = {
      std::make_shared<seq::ast::types::LinkType>(
          seq::ast::types::LinkType::Kind::Unbound, 0)};
  for (auto *t : types) {
    assert(t->getAstType());
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

const std::string IRModule::EQ_MAGIC_NAME = "__eq__";
const std::string IRModule::NE_MAGIC_NAME = "__ne__";
const std::string IRModule::LT_MAGIC_NAME = "__lt__";
const std::string IRModule::GT_MAGIC_NAME = "__gt__";
const std::string IRModule::LE_MAGIC_NAME = "__le__";
const std::string IRModule::GE_MAGIC_NAME = "__ge__";

const std::string IRModule::POS_MAGIC_NAME = "__pos__";
const std::string IRModule::NEG_MAGIC_NAME = "__neg__";
const std::string IRModule::INVERT_MAGIC_NAME = "__invert__";

const std::string IRModule::ADD_MAGIC_NAME = "__add__";
const std::string IRModule::SUB_MAGIC_NAME = "__sub__";
const std::string IRModule::MUL_MAGIC_NAME = "__mul__";
const std::string IRModule::TRUE_DIV_MAGIC_NAME = "__truediv__";
const std::string IRModule::FLOOR_DIV_MAGIC_NAME = "__floordiv__";
const std::string IRModule::MOD_MAGIC_NAME = "__mod__";
const std::string IRModule::POW_MAGIC_NAME = "__pow__";
const std::string IRModule::LSHIFT_MAGIC_NAME = "__lshift__";
const std::string IRModule::RSHIFT_MAGIC_NAME = "__rshift__";
const std::string IRModule::AND_MAGIC_NAME = "__and__";
const std::string IRModule::OR_MAGIC_NAME = "__or__";
const std::string IRModule::XOR_MAGIC_NAME = "__xor__";

const std::string IRModule::INT_MAGIC_NAME = "__int__";
const std::string IRModule::BOOL_MAGIC_NAME = "__bool__";
const std::string IRModule::STR_MAGIC_NAME = "__str__";

const std::string IRModule::GET_MAGIC_NAME = "__getitem__";
const std::string IRModule::ITER_MAGIC_NAME = "__iter__";
const std::string IRModule::LEN_MAGIC_NAME = "__len__";

const char IRModule::NodeId = 0;

IRModule::IRModule(std::string name, std::shared_ptr<ast::Cache> cache)
    : AcceptorExtend(std::move(name)), cache(std::move(cache)) {
  mainFunc = std::make_unique<BodiedFunc>("main");
  mainFunc->realize(cast<types::FuncType>(unsafeGetDummyFuncType()), {});
  mainFunc->setModule(this);
  mainFunc->setReplaceable(false);
  argVar = std::make_unique<Var>(unsafeGetArrayType(getStringType()), true, "argv");
  argVar->setModule(this);
  argVar->setReplaceable(false);
}

Func *IRModule::getOrRealizeMethod(types::Type *parent, const std::string &methodName,
                                   std::vector<types::Type *> args,
                                   std::vector<types::Generic> generics) {

  auto method = cache->findMethod(
      std::const_pointer_cast<ast::types::Type>(parent->getAstType())->getClass().get(),
      methodName, generateDummyNames(args));
  if (!method)
    return nullptr;
  return cache->realizeFunction(method, translateArgs(args),
                                translateGenerics(generics));
}

Func *IRModule::getOrRealizeFunc(const std::string &funcName,
                                 std::vector<types::Type *> args,
                                 std::vector<types::Generic> generics) {
  auto func = cache->findFunction(funcName);
  if (!func)
    return nullptr;
  auto arg = translateArgs(args);
  auto gens = translateGenerics(generics);
  return cache->realizeFunction(func, arg, gens);
}

types::Type *IRModule::getOrRealizeType(const std::string &typeName,
                                        std::vector<types::Generic> generics) {
  auto type = cache->findClass(typeName);
  if (!type)
    return nullptr;
  return cache->realizeType(type, translateGenerics(generics));
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
      std::vector<types::Type *>{getIntType(), unsafeGetPointerType(getByteType())},
      std::vector<std::string>{"len", "ptr"});
}

types::Type *IRModule::getPointerType(types::Type *base) {
  return getOrRealizeType("Pointer", {base});
}

types::Type *IRModule::getArrayType(types::Type *base) {
  return getOrRealizeType("Array", {base});
}

types::Type *IRModule::getGeneratorType(types::Type *base) {
  return getOrRealizeType("Array", {base});
}

types::Type *IRModule::getOptionalType(types::Type *base) {
  return getOrRealizeType("Array", {base});
}

types::Type *IRModule::getFuncType(types::Type *rType,
                                   std::vector<types::Type *> argTypes) {
  auto args = translateArgs(argTypes);
  args[0] = std::make_shared<seq::ast::types::LinkType>(rType->getAstType());
  return cache->makeFunction(args);
}

types::Type *IRModule::getIntNType(unsigned int len, bool sign) {
  return getOrRealizeType(sign ? "Int" : "UInt", {len});
}

types::Type *IRModule::getTupleType(std::vector<types::Type *> args) {
  std::vector<ast::types::TypePtr> argTypes;
  for (auto *t : args) {
    assert(t->getAstType());
    argTypes.push_back(t->getAstType());
  }
  return cache->makeTuple(argTypes);
}

Value *IRModule::getIntConstant(int64_t v) { return Nr<IntConstant>(v, getIntType()); }

Value *IRModule::getFloatConstant(double v) {
  return Nr<FloatConstant>(v, getFloatType());
}

Value *IRModule::getBoolConstant(bool v) { return Nr<BoolConstant>(v, getBoolType()); }

Value *IRModule::getStringConstant(std::string v) {
  return Nr<StringConstant>(std::move(v), getStringType());
}

types::Type *IRModule::unsafeGetDummyFuncType() {
  return unsafeGetFuncType("<internal_func_type>", getVoidType(), {});
}

types::Type *IRModule::unsafeGetPointerType(types::Type *base) {
  auto name = types::PointerType::getInstanceName(base);
  if (auto *rVal = getType(name))
    return rVal;
  return Nr<types::PointerType>(base);
}

types::Type *IRModule::unsafeGetArrayType(types::Type *base) {
  auto name = fmt::format(FMT_STRING(".Array[{}]"), base->referenceString());
  if (auto *rVal = getType(name))
    return rVal;
  std::vector<types::Type *> members = {getIntType(), unsafeGetPointerType(base)};
  std::vector<std::string> names = {"len", "ptr"};
  return Nr<types::RecordType>(name, members, names);
}

types::Type *IRModule::unsafeGetGeneratorType(types::Type *base) {
  auto name = types::GeneratorType::getInstanceName(base);
  if (auto *rVal = getType(name))
    return rVal;
  return Nr<types::GeneratorType>(base);
}

types::Type *IRModule::unsafeGetOptionalType(types::Type *base) {
  auto name = types::OptionalType::getInstanceName(base);
  if (auto *rVal = getType(name))
    return rVal;
  return Nr<types::OptionalType>(base);
}

types::Type *IRModule::unsafeGetFuncType(const std::string &name, types::Type *rType,
                                         std::vector<types::Type *> argTypes) {
  if (auto *rVal = getType(name))
    return rVal;
  return Nr<types::FuncType>(name, rType, std::move(argTypes));
}

types::Type *IRModule::unsafeGetMemberedType(const std::string &name, bool ref) {
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

types::Type *IRModule::unsafeGetIntNType(unsigned int len, bool sign) {
  auto name = types::IntNType::getInstanceName(len, sign);
  if (auto *rVal = getType(name))
    return rVal;
  return Nr<types::IntNType>(len, sign);
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
