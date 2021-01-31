#include "base.h"

#include "types/types.h"
#include "value.h"
#include "var.h"

namespace seq {
namespace ir {

const char IRNode::NodeId = 0;

int IRNode::replaceUsedValue(Value *old, Value *newValue) {
  return replaceUsedValue(old->getId(), newValue);
}

int IRNode::replaceUsedType(types::Type *old, types::Type *newType) {
  return replaceUsedType(old->getName(), newType);
}

int IRNode::replaceUsedVariable(Var *old, Var *newVar) {
  return replaceUsedVariable(old->getId(), newVar);
}

int IdMixin::currentId = 0;

void IdMixin::resetId() { currentId = 0; }

} // namespace ir
} // namespace seq
