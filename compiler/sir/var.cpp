#include "var.h"

#include "module.h"

namespace seq {
namespace ir {

const char Var::NodeId = 0;

Var *Var::clone() const {
  auto *res = doClone();
  for (auto it = attributes_begin(); it != attributes_end(); ++it) {
    auto *attr = getAttribute(*it);
    if (attr->needsClone())
      res->setAttribute(attr->clone(), *it);
  }
  return res;
}

Var *Var::doClone() const {
  return getModule()->N<Var>(getSrcInfo(), type, global, getName());
}

std::ostream &Var::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: {}"), referenceString(), type->referenceString());
  return os;
}

const char VarValue::NodeId = 0;

Value *VarValue::doClone() const { return getModule()->N<VarValue>(getSrcInfo(), val); }

const char PointerValue::NodeId = 0;

const types::Type *PointerValue::doGetType() const {
  return getModule()->getPointerType(const_cast<types::Type *>(val->getType()));
}

Value *PointerValue::doClone() const {
  return getModule()->N<PointerValue>(getSrcInfo(), val);
}

} // namespace ir
} // namespace seq
