#include "var.h"

#include "module.h"

namespace seq {
namespace ir {

const char Var::NodeId = 0;

types::Type *Var::getType() const { return getModule()->getPointerType(type); }

std::ostream &Var::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: {}"), referenceString(), type->referenceString());
  return os;
}

} // namespace ir
} // namespace seq
