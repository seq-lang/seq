#include "module.h"

#include "func.h"

namespace seq {
namespace ir {

const std::string IRModule::VOID_NAME = ".void";
const std::string IRModule::BOOL_NAME = ".bool";
const std::string IRModule::BYTE_NAME = ".byte";
const std::string IRModule::INT_NAME = ".int";
const std::string IRModule::FLOAT_NAME = ".float";
const std::string IRModule::STRING_NAME = ".str";

const char IRModule::NodeId = 0;

IRModule::IRModule(std::string name)
    : AcceptorExtend(std::move(name)),
      mainFunc(new BodiedFunc(getVoidRetAndArgFuncType(), "main")),
      argVar(new Var(getArrayType(getStringType()), true, "argv")) {
  mainFunc->setModule(this);
  argVar->setModule(this);
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
