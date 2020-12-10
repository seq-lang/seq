#include "var.h"

#include "util/fmt/format.h"

#include "util/visitor.h"

namespace seq {
namespace ir {

void Var::accept(util::SIRVisitor &v) { v.visit(this); }

std::ostream &Var::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: {};"), name,
             type ? type->referenceString() : "internal");
  return os;
}

} // namespace ir
} // namespace seq
