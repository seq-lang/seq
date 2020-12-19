#include "types.h"

#include <algorithm>
#include <utility>

#include "util/fmt/format.h"

#include "sir/util/iterators.h"
#include "sir/util/visitor.h"

namespace seq {
namespace ir {
namespace types {

std::ostream &Type::doFormat(std::ostream &os) const { return os << referenceString(); }

RecordType::RecordType(std::string name, std::vector<Type *> fieldTypes,
                       std::vector<std::string> fieldNames)
    : TypeBase(std::move(name)) {
  for (auto i = 0; i < fieldTypes.size(); ++i) {
    fields.emplace_back(fieldNames[i], fieldTypes[i]);
  }
}

RecordType::RecordType(std::string name, std::vector<Type *> mTypes)
    : TypeBase(std::move(name)) {
  for (int i = 0; i < mTypes.size(); ++i) {
    fields.emplace_back(std::to_string(i + 1), mTypes[i]);
  }
}

Type *RecordType::getMemberType(const std::string &n) {
  auto it =
      std::find_if(fields.begin(), fields.end(), [n](auto &x) { return x.name == n; });
  return it->type;
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
    fmt::print(os, FMT_STRING("{}: {}{}"), fields[i].name,
               fields[i].type->referenceString(), sep);
  }
  os << ')';
  return os;
}

std::ostream &RefType::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: ref({})"), referenceString(), *contents);
  return os;
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

std::string FuncType::getName(Type *rType, const std::vector<Type *> &argTypes) {
  auto wrap = [](auto it) -> auto {
    auto f = [](auto it) { return it->referenceString(); };
    return util::function_iterator_adaptor<decltype(it), decltype(f)>(it, std::move(f));
  };

  return fmt::format(FMT_STRING(".Function.{}[{}]"), rType->referenceString(),
                     fmt::join(wrap(argTypes.begin()), wrap(argTypes.end()), ", "));
}

std::string PointerType::getName(Type *base) {
  return fmt::format(FMT_STRING(".Pointer[{}]"), base->referenceString());
}

OptionalType::OptionalType(Type *pointerType, Type *flagType)
    : TypeBase(getName(dynamic_cast<PointerType *>(pointerType)->getBase()),
               std::vector<Type *>{flagType, pointerType},
               std::vector<std::string>{"has", "val"}),
      base(dynamic_cast<PointerType *>(pointerType)->getBase()) {}

std::string OptionalType::getName(Type *base) {
  return fmt::format(FMT_STRING(".Optional[{}]"), base->referenceString());
}

ArrayType::ArrayType(Type *pointerType, Type *countType)
    : TypeBase(getName(dynamic_cast<PointerType *>(pointerType)->getBase()),
               std::vector<Type *>{countType, pointerType},
               std::vector<std::string>{"len", "ptr"}),
      base(dynamic_cast<PointerType *>(pointerType)->getBase()) {}

std::string ArrayType::getName(Type *base) {
  return fmt::format(FMT_STRING(".Array[{}]"), base->referenceString());
}

std::string GeneratorType::getName(Type *base) {
  return fmt::format(FMT_STRING(".Generator[{}]"), base->referenceString());
}

std::string IntNType::getName(unsigned int len, bool sign) {
  return fmt::format(FMT_STRING(".{}Int{}"), sign ? "" : "U", len);
}

} // namespace types
} // namespace ir
} // namespace seq