#include "module.h"

#include <memory>

#include <algorithm>

#include "func.h"

namespace {

using namespace seq::ir;

bool argMatch(types::FuncType *f, const std::vector<types::Type *> &argTypes) {
  if (std::distance(f->begin(), f->begin()) != argTypes.size())
    return false;

  auto it1 = f->begin();
  auto it2 = argTypes.begin();

  while (it1 != f->end()) {
    if (!(*it1)->equals(*it2))
      return false;

    ++it1;
    ++it2;
  }

  return true;
}

} // namespace

namespace seq {
namespace ir {

const std::string IRModule::VOID_NAME = ".void";
const std::string IRModule::BOOL_NAME = ".bool";
const std::string IRModule::BYTE_NAME = ".byte";
const std::string IRModule::INT_NAME = ".int";
const std::string IRModule::FLOAT_NAME = ".float";
const std::string IRModule::STRING_NAME = ".str";

const char IRModule::NodeId = 0;

IRModule::IRModule(std::string name) : AcceptorExtend(std::move(name)) {
  mainFunc = std::make_unique<BodiedFunc>(getVoidRetAndArgFuncType(), "main");
  mainFunc->setModule(this);
  argVar = std::make_unique<Var>(getArrayType(getStringType()), true, "argv");
  argVar->setModule(this);
}

Func *IRModule::lookupFunc(const std::string &name,
                           const std::vector<types::Type *> &argTypes) {
  auto it = std::find_if(begin(), end(), [name, argTypes](Var *v) {
    return isA<Func>(v) && name == v->getName() &&
           argMatch(cast<types::FuncType>(v->getType()), argTypes);
  });
  return it != end() ? cast<Func>(*it) : nullptr;
}

Func *IRModule::lookupFunc(const std::string &name) {
  auto it = std::find_if(
      begin(), end(), [name](Var *v) { return isA<Func>(v) && name == v->getName(); });
  return it != end() ? cast<Func>(*it) : nullptr;
}

BodiedFunc *IRModule::lookupBuiltinFunc(const std::string &name) {
  auto it = std::find_if(begin(), end(), [name](Var *v) {
    auto *f = cast<BodiedFunc>(v);
    return f && f->isBuiltin() && f->getUnmangledName() == name;
  });

  return it != end() ? cast<BodiedFunc>(*it) : nullptr;
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
