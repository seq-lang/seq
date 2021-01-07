#include "types.h"

#include <algorithm>
#include <utility>

#include "util/fmt/format.h"

#include "sir/util/iterators.h"
#include "sir/util/visitor.h"

namespace seq {
namespace ir {
namespace types {

const char Type::NodeId = 0;

std::ostream &Type::doFormat(std::ostream &os) const { return os << referenceString(); }

const char PrimitiveType::NodeId = 0;

const char IntType::NodeId = 0;

const char FloatType::NodeId = 0;

const char BoolType::NodeId = 0;

const char ByteType::NodeId = 0;

const char VoidType::NodeId = 0;

const char MemberedType::NodeId = 0;

const char RecordType::NodeId = 0;

RecordType::RecordType(std::string name, std::vector<const Type *> fieldTypes,
                       std::vector<std::string> fieldNames)
    : AcceptorExtend(std::move(name)) {
  for (auto i = 0; i < fieldTypes.size(); ++i) {
    fields.emplace_back(fieldNames[i], fieldTypes[i]);
  }
}

RecordType::RecordType(std::string name, std::vector<const Type *> mTypes)
    : AcceptorExtend(std::move(name)) {
  for (int i = 0; i < mTypes.size(); ++i) {
    fields.emplace_back(std::to_string(i + 1), mTypes[i]);
  }
}

const Type *RecordType::getMemberType(const std::string &n) const {
  auto it =
      std::find_if(fields.begin(), fields.end(), [n](auto &x) { return x.name == n; });
  return it->type;
}

int RecordType::getMemberIndex(const std::string &n) const {
  auto it =
      std::find_if(fields.begin(), fields.end(), [n](auto &x) { return x.name == n; });
  size_t index = std::distance(fields.begin(), it);
  return (index < fields.size()) ? index : -1;
}

void RecordType::realize(std::vector<const Type *> mTypes,
                         std::vector<std::string> mNames) {
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

const char RefType::NodeId = 0;

std::ostream &RefType::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: ref({})"), referenceString(), *contents);
  return os;
}

const char FuncType::NodeId = 0;

std::ostream &FuncType::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: ("), referenceString());
  for (auto it = argTypes.begin(); it != argTypes.end(); ++it) {
    auto sep = it + 1 != argTypes.end() ? ", " : "";
    fmt::print(os, FMT_STRING("{}{}"), (*it)->referenceString(), sep);
  }
  fmt::print(os, FMT_STRING(")->{}"), rType->referenceString());
  return os;
}

std::string FuncType::getName(const Type *rType,
                              const std::vector<const Type *> &argTypes) {
  auto wrap = [](auto it) -> auto {
    auto f = [](auto it) { return it->referenceString(); };
    auto m = [](auto it) { return nullptr; };
    return util::function_iterator_adaptor<decltype(it), decltype(f), decltype(m)>(
        it, std::move(f), std::move(m));
  };

  return fmt::format(FMT_STRING(".Function.{}[{}]"), rType->referenceString(),
                     fmt::join(wrap(argTypes.begin()), wrap(argTypes.end()), ", "));
}

const char DerivedType::NodeId = 0;

const char PointerType::NodeId = 0;

std::string PointerType::getName(const Type *base) {
  return fmt::format(FMT_STRING(".Pointer[{}]"), base->referenceString());
}

const char OptionalType::NodeId = 0;

std::string OptionalType::getName(const Type *base) {
  return fmt::format(FMT_STRING(".Optional[{}]"), base->referenceString());
}

const char ArrayType::NodeId = 0;

ArrayType::ArrayType(const Type *pointerType, const Type *countType)
    : AcceptorExtend(getName(cast<PointerType>(pointerType)->getBase()),
                     std::vector<const Type *>{countType, pointerType},
                     std::vector<std::string>{"len", "ptr"}),
      base(cast<PointerType>(pointerType)->getBase()) {}

std::string ArrayType::getName(const Type *base) {
  return fmt::format(FMT_STRING(".Array[{}]"), base->referenceString());
}

const char GeneratorType::NodeId = 0;

std::string GeneratorType::getName(const Type *base) {
  return fmt::format(FMT_STRING(".Generator[{}]"), base->referenceString());
}

const char IntNType::NodeId = 0;

std::string IntNType::getName(unsigned int len, bool sign) {
  return fmt::format(FMT_STRING(".{}Int{}"), sign ? "" : "U", len);
}

} // namespace types
} // namespace ir
} // namespace seq
