#include "module.h"

#include <algorithm>
#include <memory>

#include "parser/cache.h"

#include "func.h"

namespace seq {
namespace ir {
namespace {
std::vector<seq::ast::types::TypePtr>
translateGenerics(std::vector<types::Generic> &generics) {
  std::vector<seq::ast::types::TypePtr> ret;
  for (auto &g : generics) {
    seqassert(g.isStatic() || g.getTypeValue(), "generic must be static or a type");
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
    seqassert(t->getAstType(), "{} must have an ast type", *t);
    ret.emplace_back("", t->getAstType());
  }
  return ret;
}

std::vector<seq::ast::types::TypePtr> translateArgs(std::vector<types::Type *> &types) {
  std::vector<seq::ast::types::TypePtr> ret = {
      std::make_shared<seq::ast::types::LinkType>(
          seq::ast::types::LinkType::Kind::Unbound, 0)};
  for (auto *t : types) {
    seqassert(t->getAstType(), "{} must have an ast type", *t);
    if (auto f = t->getAstType()->getFunc()) {
      auto *irType = cast<types::FuncType>(t);
      std::vector<char> mask(std::distance(irType->begin(), irType->end()), 0);
      ret.push_back(std::make_shared<seq::ast::types::PartialType>(
          t->getAstType()->getRecord(), f, mask));
    } else {
      ret.push_back(t->getAstType());
    }
  }
  return ret;
}
} // namespace

const std::string Module::VOID_NAME = "void";
const std::string Module::BOOL_NAME = "bool";
const std::string Module::BYTE_NAME = "byte";
const std::string Module::INT_NAME = "int";
const std::string Module::FLOAT_NAME = "float";
const std::string Module::STRING_NAME = "str";

const std::string Module::EQ_MAGIC_NAME = "__eq__";
const std::string Module::NE_MAGIC_NAME = "__ne__";
const std::string Module::LT_MAGIC_NAME = "__lt__";
const std::string Module::GT_MAGIC_NAME = "__gt__";
const std::string Module::LE_MAGIC_NAME = "__le__";
const std::string Module::GE_MAGIC_NAME = "__ge__";

const std::string Module::POS_MAGIC_NAME = "__pos__";
const std::string Module::NEG_MAGIC_NAME = "__neg__";
const std::string Module::INVERT_MAGIC_NAME = "__invert__";

const std::string Module::ADD_MAGIC_NAME = "__add__";
const std::string Module::SUB_MAGIC_NAME = "__sub__";
const std::string Module::MUL_MAGIC_NAME = "__mul__";
const std::string Module::TRUE_DIV_MAGIC_NAME = "__truediv__";
const std::string Module::FLOOR_DIV_MAGIC_NAME = "__floordiv__";
const std::string Module::MOD_MAGIC_NAME = "__mod__";
const std::string Module::POW_MAGIC_NAME = "__pow__";
const std::string Module::LSHIFT_MAGIC_NAME = "__lshift__";
const std::string Module::RSHIFT_MAGIC_NAME = "__rshift__";
const std::string Module::AND_MAGIC_NAME = "__and__";
const std::string Module::OR_MAGIC_NAME = "__or__";
const std::string Module::XOR_MAGIC_NAME = "__xor__";

const std::string Module::INT_MAGIC_NAME = "__int__";
const std::string Module::FLOAT_MAGIC_NAME = "__float__";
const std::string Module::BOOL_MAGIC_NAME = "__bool__";
const std::string Module::STR_MAGIC_NAME = "__str__";

const std::string Module::GETITEM_MAGIC_NAME = "__getitem__";
const std::string Module::SETITEM_MAGIC_NAME = "__setitem__";
const std::string Module::ITER_MAGIC_NAME = "__iter__";
const std::string Module::LEN_MAGIC_NAME = "__len__";

const std::string Module::NEW_MAGIC_NAME = "__new__";
const std::string Module::INIT_MAGIC_NAME = "__init__";

const char Module::NodeId = 0;

Module::Module(std::string name, std::shared_ptr<ast::Cache> cache)
    : AcceptorExtend(std::move(name)), cache(std::move(cache)) {
  mainFunc = std::make_unique<BodiedFunc>("main");
  mainFunc->realize(cast<types::FuncType>(unsafeGetDummyFuncType()), {});
  mainFunc->setModule(this);
  mainFunc->setReplaceable(false);
  argVar = std::make_unique<Var>(unsafeGetArrayType(getStringType()), true, "argv");
  argVar->setModule(this);
  argVar->setReplaceable(false);
}

Func *Module::getOrRealizeMethod(types::Type *parent, const std::string &methodName,
                                 std::vector<types::Type *> args,
                                 std::vector<types::Generic> generics) {

  auto cls =
      std::const_pointer_cast<ast::types::Type>(parent->getAstType())->getClass();
  auto method = cache->findMethod(cls.get(), methodName, generateDummyNames(args));
  if (!method)
    return nullptr;
  return cache->realizeFunction(method, translateArgs(args),
                                translateGenerics(generics), cls);
}

Func *Module::getOrRealizeFunc(const std::string &funcName,
                               std::vector<types::Type *> args,
                               std::vector<types::Generic> generics,
                               const std::string &module) {
  auto fqName =
      module.empty() ? funcName : fmt::format(FMT_STRING("{}.{}"), module, funcName);
  auto func = cache->findFunction(fqName);
  if (!func)
    return nullptr;
  auto arg = translateArgs(args);
  auto gens = translateGenerics(generics);
  return cache->realizeFunction(func, arg, gens);
}

types::Type *Module::getOrRealizeType(const std::string &typeName,
                                      std::vector<types::Generic> generics,
                                      const std::string &module) {
  auto fqName =
      module.empty() ? typeName : fmt::format(FMT_STRING("{}.{}"), module, typeName);
  auto type = cache->findClass(fqName);
  if (!type)
    return nullptr;
  return cache->realizeType(type, translateGenerics(generics));
}

types::Type *Module::getVoidType() {
  if (auto *rVal = getType(VOID_NAME))
    return rVal;
  return Nr<types::VoidType>();
}

types::Type *Module::getBoolType() {
  if (auto *rVal = getType(BOOL_NAME))
    return rVal;
  return Nr<types::BoolType>();
}

types::Type *Module::getByteType() {
  if (auto *rVal = getType(BYTE_NAME))
    return rVal;
  return Nr<types::ByteType>();
}

types::Type *Module::getIntType() {
  if (auto *rVal = getType(INT_NAME))
    return rVal;
  return Nr<types::IntType>();
}

types::Type *Module::getFloatType() {
  if (auto *rVal = getType(FLOAT_NAME))
    return rVal;
  return Nr<types::FloatType>();
}

types::Type *Module::getStringType() {
  if (auto *rVal = getType(STRING_NAME))
    return rVal;
  return Nr<types::RecordType>(
      STRING_NAME,
      std::vector<types::Type *>{getIntType(), unsafeGetPointerType(getByteType())},
      std::vector<std::string>{"len", "ptr"});
}

types::Type *Module::getPointerType(types::Type *base) {
  return getOrRealizeType("Ptr", {base});
}

types::Type *Module::getArrayType(types::Type *base) {
  return getOrRealizeType("Array", {base});
}

types::Type *Module::getGeneratorType(types::Type *base) {
  return getOrRealizeType("Generator", {base});
}

types::Type *Module::getOptionalType(types::Type *base) {
  return getOrRealizeType("Optional", {base});
}

types::Type *Module::getFuncType(types::Type *rType,
                                 std::vector<types::Type *> argTypes) {
  auto args = translateArgs(argTypes);
  args[0] = std::make_shared<seq::ast::types::LinkType>(rType->getAstType());
  return cache->makeFunction(args);
}

types::Type *Module::getIntNType(unsigned int len, bool sign) {
  return getOrRealizeType(sign ? "Int" : "UInt", {len});
}

types::Type *Module::getTupleType(std::vector<types::Type *> args) {
  std::vector<ast::types::TypePtr> argTypes;
  for (auto *t : args) {
    seqassert(t->getAstType(), "{} must have an ast type", *t);
    argTypes.push_back(t->getAstType());
  }
  return cache->makeTuple(argTypes);
}

Value *Module::getInt(int64_t v) { return Nr<IntConst>(v, getIntType()); }

Value *Module::getFloat(double v) { return Nr<FloatConst>(v, getFloatType()); }

Value *Module::getBool(bool v) { return Nr<BoolConst>(v, getBoolType()); }

Value *Module::getString(std::string v) {
  return Nr<StringConst>(std::move(v), getStringType());
}

types::Type *Module::unsafeGetDummyFuncType() {
  return unsafeGetFuncType("<internal_func_type>", getVoidType(), {});
}

types::Type *Module::unsafeGetPointerType(types::Type *base) {
  auto name = types::PointerType::getInstanceName(base);
  if (auto *rVal = getType(name))
    return rVal;
  return Nr<types::PointerType>(base);
}

types::Type *Module::unsafeGetArrayType(types::Type *base) {
  auto name = fmt::format(FMT_STRING(".Array[{}]"), base->referenceString());
  if (auto *rVal = getType(name))
    return rVal;
  std::vector<types::Type *> members = {getIntType(), unsafeGetPointerType(base)};
  std::vector<std::string> names = {"len", "ptr"};
  return Nr<types::RecordType>(name, members, names);
}

types::Type *Module::unsafeGetGeneratorType(types::Type *base) {
  auto name = types::GeneratorType::getInstanceName(base);
  if (auto *rVal = getType(name))
    return rVal;
  return Nr<types::GeneratorType>(base);
}

types::Type *Module::unsafeGetOptionalType(types::Type *base) {
  auto name = types::OptionalType::getInstanceName(base);
  if (auto *rVal = getType(name))
    return rVal;
  return Nr<types::OptionalType>(base);
}

types::Type *Module::unsafeGetFuncType(const std::string &name, types::Type *rType,
                                       std::vector<types::Type *> argTypes) {
  if (auto *rVal = getType(name))
    return rVal;
  return Nr<types::FuncType>(name, rType, std::move(argTypes));
}

types::Type *Module::unsafeGetMemberedType(const std::string &name, bool ref) {
  auto *rVal = getType(name);

  if (!rVal) {
    if (ref) {
      auto contentName = name + ".contents";
      auto *record = getType(contentName);
      if (!record) {
        record = Nr<types::RecordType>(contentName);
      }
      rVal = Nr<types::RefType>(name, cast<types::RecordType>(record));
    } else {
      rVal = Nr<types::RecordType>(name);
    }
  }

  return rVal;
}

types::Type *Module::unsafeGetIntNType(unsigned int len, bool sign) {
  auto name = types::IntNType::getInstanceName(len, sign);
  if (auto *rVal = getType(name))
    return rVal;
  return Nr<types::IntNType>(len, sign);
}

} // namespace ir
} // namespace seq
