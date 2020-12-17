#include "var.h"

namespace seq {
namespace ir {

std::ostream &Var::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: {}"), referenceString(), type->referenceString());
  return os;
}

} // namespace ir
} // namespace seq
