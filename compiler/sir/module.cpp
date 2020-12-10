#include "util/fmt/format.h"

#include "func.h"
#include "module.h"
#include "var.h"

#include "util/visitor.h"

namespace seq {
namespace ir {

SIRModule::SIRModule(std::string name)
    : AttributeHolder(std::move(name)), globals(this), types(this),
      mainFunc(std::make_unique<Func>("main")) {
  mainFunc->parent = this;
}

void SIRModule::accept(util::SIRVisitor &v) { v.visit(this); }

std::ostream &SIRModule::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("module {}{{\n"), name);
  fmt::print(os, "{}\n", *mainFunc);

  for (auto &g : globals) {
    fmt::print(os, FMT_STRING("{}\n"), *g);
  }
  os << '}';
  return os;
}

} // namespace ir
} // namespace seq
