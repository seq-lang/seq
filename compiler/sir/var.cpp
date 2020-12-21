#include "var.h"

#include "module.h"

namespace seq {
namespace ir {

const char Var::NodeId = 0;

std::ostream &Var::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: {}"), referenceString(), type->referenceString());
  return os;
}

const char VarValue::NodeId = 0;

const char PointerValue::NodeId = 0;

types::Type *PointerValue::getType() const {
  return getModule()->getPointerType(val->getType());
}

} // namespace ir
} // namespace seq
