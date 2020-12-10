#include "util/fmt/format.h"

#include "lvalue.h"
#include "var.h"

#include "util/visitor.h"

namespace seq {
namespace ir {

void Lvalue::accept(util::SIRVisitor &v) { v.visit(this); }

void VarLvalue::accept(util::SIRVisitor &v) { v.visit(this); }

types::Type *VarLvalue::getType() const { return var->type; }

std::ostream &VarLvalue::doFormat(std::ostream &os) const {
  return os << var->referenceString();
}

void VarMemberLvalue::accept(util::SIRVisitor &v) { v.visit(this); }

types::Type *VarMemberLvalue::getType() const {
  return dynamic_cast<types::MemberedType *>(var->type)->getMemberType(field);
}

std::ostream &VarMemberLvalue::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}.{}"), var->referenceString(), field);
  return os;
}

} // namespace ir
} // namespace seq
