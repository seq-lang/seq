#include "types.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "util/fmt/format.h"

#include "parser/cache.h"

#include "sir/module.h"
#include "sir/util/iterators.h"
#include "sir/util/visitor.h"

namespace {
std::vector<seq::ast::types::TypePtr>
extractTypes(const std::vector<seq::ast::types::Generic> &gens) {
  std::vector<seq::ast::types::TypePtr> ret;
  for (auto &g : gens)
    ret.push_back(g.type);
  return ret;
}
} // namespace

namespace seq {
namespace ir {
namespace types {

const char Type::NodeId = 0;

Type *Type::clone() const {
  if (hasReplacement())
    return getActual()->clone();

  auto *res = doClone();
  for (auto it = attributes_begin(); it != attributes_end(); ++it) {
    auto *attr = getAttribute(*it);
    if (attr->needsClone())
      res->setAttribute(attr->clone(), *it);
  }
  res->setAstType(std::const_pointer_cast<ast::types::Type>(getAstType()));
  return res;
}

std::vector<Generic> Type::getGenerics() {
  if (!astType)
    return {};

  std::vector<Generic> ret;
  for (auto &g : astType->getClass()->generics) {
    auto bound = g.type->getLink();
    if (auto cls = bound->type->getClass())
      ret.emplace_back(
          getModule()->getCache()->realizeType(cls, extractTypes(cls->generics)));
    else
      ret.emplace_back(bound->type->getStatic()->staticEvaluation.second);
  }

  return ret;
}

std::ostream &Type::doFormat(std::ostream &os) const { return os << referenceString(); }

const char PrimitiveType::NodeId = 0;

const char IntType::NodeId = 0;

IntType *IntType::doClone() const { return cast<IntType>(getModule()->getIntType()); }

const char FloatType::NodeId = 0;

FloatType *FloatType::doClone() const {
  return cast<FloatType>(getModule()->getFloatType());
}

const char BoolType::NodeId = 0;

BoolType *BoolType::doClone() const {
  return cast<BoolType>(getModule()->getBoolType());
}

const char ByteType::NodeId = 0;

ByteType *ByteType::doClone() const {
  return cast<ByteType>(getModule()->getByteType());
}

const char VoidType::NodeId = 0;

VoidType *VoidType::doClone() const {
  return cast<VoidType>(getModule()->getVoidType());
}

const char MemberedType::NodeId = 0;

const char RecordType::NodeId = 0;

RecordType::RecordType(std::string name, std::vector<Type *> fieldTypes,
                       std::vector<std::string> fieldNames)
    : AcceptorExtend(std::move(name)) {
  for (auto i = 0; i < fieldTypes.size(); ++i) {
    fields.emplace_back(fieldNames[i], fieldTypes[i]);
  }
}

RecordType::RecordType(std::string name, std::vector<Type *> mTypes)
    : AcceptorExtend(std::move(name)) {
  for (int i = 0; i < mTypes.size(); ++i) {
    fields.emplace_back(std::to_string(i + 1), mTypes[i]);
  }
}

std::vector<Type *> RecordType::doGetUsedTypes() const {
  std::vector<Type *> ret;
  for (auto &f : fields)
    ret.push_back(const_cast<Type *>(f.getType()));
  return ret;
}

int RecordType::doReplaceUsedType(const std::string &name, Type *newType) {
  auto count = 0;
  for (auto &f : fields)
    if (f.getType()->getName() == name) {
      f.setType(newType);
      ++count;
    }
  return count;
}

Type *RecordType::getMemberType(const std::string &n) {
  auto it = std::find_if(fields.begin(), fields.end(),
                         [n](auto &x) { return x.getName() == n; });
  return it->getType();
}

const Type *RecordType::getMemberType(const std::string &n) const {
  auto it = std::find_if(fields.begin(), fields.end(),
                         [n](auto &x) { return x.getName() == n; });
  return it->getType();
}

int RecordType::getMemberIndex(const std::string &n) const {
  auto it = std::find_if(fields.begin(), fields.end(),
                         [n](auto &x) { return x.getName() == n; });
  int index = std::distance(fields.begin(), it);
  return (index < fields.size()) ? index : -1;
}

void RecordType::realize(std::vector<Type *> mTypes, std::vector<std::string> mNames) {
  fields.clear();
  for (auto i = 0; i < mTypes.size(); ++i) {
    fields.emplace_back(mNames[i], mTypes[i]);
  }
}

RecordType *RecordType::doClone() const {
  std::vector<Type *> fieldTypes;
  std::vector<std::string> fieldNames;
  for (const auto &field : *this) {
    fieldTypes.push_back(field.getType()->clone());
    fieldNames.push_back(field.getName());
  }
  return getModule()->N<RecordType>(getSrcInfo(), getName(), fieldTypes, fieldNames);
}

std::ostream &RecordType::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: ("), referenceString());
  for (auto i = 0; i < fields.size(); ++i) {
    auto sep = i + 1 != fields.size() ? ", " : "";
    fmt::print(os, FMT_STRING("{}: {}{}"), fields[i].getName(),
               fields[i].getType()->referenceString(), sep);
  }
  os << ')';
  return os;
}

const char RefType::NodeId = 0;

std::ostream &RefType::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: ref({})"), referenceString(), *contents);
  return os;
}

int RefType::doReplaceUsedType(const std::string &name, Type *newType) {
  if (contents->getName() == name) {
    auto *record = cast<RecordType>(newType);
    assert(record);
    contents = record;
    return 1;
  }
  return 0;
}

RefType *RefType::doClone() const {
  return getModule()->N<RefType>(getSrcInfo(), getName(),
                                 cast<RecordType>(getContents()->clone()));
}

const char FuncType::NodeId = 0;

std::vector<Type *> FuncType::doGetUsedTypes() const {
  auto ret = argTypes;
  ret.push_back(rType);
  return ret;
}

int FuncType::doReplaceUsedType(const std::string &name, Type *newType) {
  auto count = 0;
  if (rType->getName() == name) {
    rType = newType;
    ++count;
  }
  for (auto &a : argTypes)
    if (a->getName() == name) {
      a = newType;
      ++count;
    }
  return count;
}

FuncType *FuncType::doClone() const {
  std::vector<Type *> argTypes;
  for (const auto *type : *this) {
    argTypes.push_back(type->clone());
  }
  return getModule()->N<FuncType>(getSrcInfo(), getName(), getReturnType()->clone(),
                                  argTypes);
}

std::ostream &FuncType::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: ("), referenceString());
  for (auto it = argTypes.begin(); it != argTypes.end(); ++it) {
    auto sep = it + 1 != argTypes.end() ? ", " : "";
    fmt::print(os, FMT_STRING("{}{}"), (*it)->referenceString(), sep);
  }
  fmt::print(os, FMT_STRING(")->{}"), rType->referenceString());
  return os;
}

const char DerivedType::NodeId = 0;

int DerivedType::doReplaceUsedType(const std::string &name, Type *newType) {
  if (base->getName() == name) {
    base = newType;
    return 1;
  }
  return 0;
}

DerivedType *DerivedType::doClone() const {
  return getModule()->N<DerivedType>(getSrcInfo(), getName(), getBase()->clone());
}

const char PointerType::NodeId = 0;

std::string PointerType::getInstanceName(Type *base) {
  return fmt::format(FMT_STRING(".Pointer[{}]"), base->referenceString());
}

PointerType *PointerType::doClone() const {
  return getModule()->N<PointerType>(getSrcInfo(), getBase()->clone());
}

const char OptionalType::NodeId = 0;

std::string OptionalType::getInstanceName(Type *base) {
  return fmt::format(FMT_STRING(".Optional[{}]"), base->referenceString());
}

OptionalType *OptionalType::doClone() const {
  return getModule()->N<OptionalType>(getSrcInfo(), getBase()->clone());
}

const char GeneratorType::NodeId = 0;

std::string GeneratorType::getInstanceName(Type *base) {
  return fmt::format(FMT_STRING(".Generator[{}]"), base->referenceString());
}

GeneratorType *GeneratorType::doClone() const {
  return getModule()->N<GeneratorType>(getSrcInfo(), getBase()->clone());
}

const char IntNType::NodeId = 0;

std::string IntNType::getInstanceName(unsigned int len, bool sign) {
  return fmt::format(FMT_STRING(".{}Int{}"), sign ? "" : "U", len);
}

IntNType *IntNType::doClone() const {
  return getModule()->N<IntNType>(getSrcInfo(), getLen(), isSigned());
}

} // namespace types
} // namespace ir
} // namespace seq
