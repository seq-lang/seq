#include "util/fmt/format.h"

#include "operand.h"
#include "var.h"

#include "util/visitor.h"

namespace seq {
namespace ir {

void Operand::accept(util::SIRVisitor &v) { v.visit(this); }

void VarOperand::accept(util::SIRVisitor &v) { v.visit(this); }

types::Type *VarOperand::getType() const { return var->type; }

std::ostream &VarOperand::doFormat(std::ostream &os) const {
  return os << var->referenceString();
}

void VarPointerOperand::accept(util::SIRVisitor &v) { v.visit(this); }

std::ostream &VarPointerOperand::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("&({})"), var->referenceString());
  return os;
}

void LiteralOperand::accept(util::SIRVisitor &v) { v.visit(this); }

std::ostream &LiteralOperand::doFormat(std::ostream &os) const {
  switch (literalType) {
  case LiteralType::FLOAT:
    return os << fval;
  case LiteralType::BOOL:
    return os << (bval ? "true" : "false");
  case LiteralType::INT:
    return os << ival;
  case LiteralType::UINT:
    return os << ival << 'u';
  case LiteralType::STR:
    fmt::print(os, FMT_STRING("'{}'"), sval);
    return os;
  default:
    return os << "<?>";
  }
}

} // namespace ir
} // namespace seq
