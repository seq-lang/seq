#include "types.h"

#include <algorithm>
#include <utility>

#include "util/fmt/format.h"

#include "sir/util/visitor.h"

namespace seq {
namespace ir {
namespace types {

void Type::accept(util::SIRVisitor &v) { v.visit(this); }

std::string Type::referenceString() const {
  return fmt::format(FMT_STRING("{}.{}"), parent ? parent->referenceString() : "",
                     name);
}

std::ostream &Type::doFormat(std::ostream &os) const { return os << name; }

RecordType::RecordType(std::string name, std::vector<Type *> fieldTypes,
                       std::vector<std::string> fieldNames)
    : MemberedType(std::move(name)) {
  for (auto i = 0; i < fieldTypes.size(); ++i) {
    fields.emplace_back(fieldNames[i], fieldTypes[i]);
  }
}

RecordType::RecordType(std::string name, std::vector<Type *> mTypes)
    : MemberedType(std::move(name)) {
  for (int i = 0; i < mTypes.size(); ++i) {
    fields.emplace_back(std::to_string(i + 1), mTypes[i]);
  }
}

void RecordType::accept(util::SIRVisitor &v) { v.visit(this); }

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
  fmt::print(os, FMT_STRING("{}: ("), name);
  for (auto i = 0; i < fields.size(); ++i) {
    auto sep = i + 1 != fields.size() ? ", " : "";
    fmt::print(os, FMT_STRING("{}: {}{}"), fields[i].name,
               fields[i].type->referenceString(), sep);
  }
  os << ')';
  return os;
}

void RefType::accept(util::SIRVisitor &v) { v.visit(this); }

std::ostream &RefType::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: ref({})"), name, *contents);
  return os;
}

void FuncType::accept(util::SIRVisitor &v) { v.visit(this); }

std::ostream &FuncType::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: ("), name);
  for (auto it = argTypes.begin(); it != argTypes.end(); ++it) {
    auto sep = it + 1 != argTypes.end() ? ", " : "";
    fmt::print(os, FMT_STRING("{}{}"), (*it)->referenceString(), sep);
  }
  fmt::print(os, FMT_STRING(")->{}"), rType->referenceString());
  return os;
}

PointerType::PointerType(Type *base)
    : Type(fmt::format(FMT_STRING(".Pointer[{}]"), base->name)), base(base) {}

void PointerType::accept(util::SIRVisitor &v) { v.visit(this); }

OptionalType::OptionalType(PointerType *pointerBase, Type *flagType)
    : RecordType(fmt::format(FMT_STRING(".Optional[{}]"), pointerBase->base->name),
                 {flagType, pointerBase}, {"has", "val"}),
      base(pointerBase->base) {}

void OptionalType::accept(util::SIRVisitor &v) { v.visit(this); }

ArrayType::ArrayType(PointerType *pointerBase, Type *countType)
    : RecordType(fmt::format(FMT_STRING(".Array[{}]"), pointerBase->base->name),
                 {countType, pointerBase}, {"len", "ptr"}),
      base(pointerBase->base) {}

void ArrayType::accept(util::SIRVisitor &v) { v.visit(this); }

GeneratorType::GeneratorType(Type *base)
    : Type(fmt::format(FMT_STRING(".Generator[{}]"), base->name)), base(base) {}

void GeneratorType::accept(util::SIRVisitor &v) { v.visit(this); }

IntNType::IntNType(unsigned int len, bool sign)
    : Type(fmt::format(FMT_STRING(".{}Int{}"), sign ? "" : "U", len)), len(len),
      sign(sign) {}

void IntNType::accept(util::SIRVisitor &v) { v.visit(this); }

std::string IntNType::oppositeSignName() const {
  return fmt::format(FMT_STRING(".{}Int{}"), sign ? "U" : "", len);
}

} // namespace types
} // namespace ir
} // namespace seq