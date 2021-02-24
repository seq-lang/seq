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

std::ostream &Type::doFormat(std::ostream &os) const { return os << referenceString(); }

std::vector<Generic> Type::doGetGenerics() const {
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

const char PrimitiveType::NodeId = 0;

const char IntType::NodeId = 0;

const char FloatType::NodeId = 0;

const char BoolType::NodeId = 0;

const char ByteType::NodeId = 0;

const char VoidType::NodeId = 0;

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

Type *RecordType::getMemberType(const std::string &n) const {
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

const char FuncType::NodeId = 0;

std::vector<Generic> FuncType::doGetGenerics() const {
  auto t = getAstType();
  if (!t)
    return {};
  auto astType = t->getFunc();
  if (!astType)
    return {};

  std::vector<Generic> ret;
  for (auto &g : astType->funcGenerics) {
    auto bound = g.type->getLink();
    if (auto cls = bound->type->getClass())
      ret.emplace_back(
          getModule()->getCache()->realizeType(cls, extractTypes(cls->generics)));
    else
      ret.emplace_back(bound->type->getStatic()->staticEvaluation.second);
  }

  return ret;
}

std::vector<Type *> FuncType::doGetUsedTypes() const {
  auto ret = argTypes;
  ret.push_back(rType);
  return ret;
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

const char PointerType::NodeId = 0;

std::string PointerType::getInstanceName(Type *base) {
  return fmt::format(FMT_STRING("Pointer[{}]"), base->referenceString());
}

const char OptionalType::NodeId = 0;

std::string OptionalType::getInstanceName(Type *base) {
  return fmt::format(FMT_STRING("Optional[{}]"), base->referenceString());
}

const char GeneratorType::NodeId = 0;

std::string GeneratorType::getInstanceName(Type *base) {
  return fmt::format(FMT_STRING("Generator[{}]"), base->referenceString());
}

const char IntNType::NodeId = 0;

std::string IntNType::getInstanceName(unsigned int len, bool sign) {
  return fmt::format(FMT_STRING("{}Int{}"), sign ? "" : "U", len);
}

} // namespace types
} // namespace ir
} // namespace seq
