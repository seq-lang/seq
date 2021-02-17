#include "value.h"

#include "module.h"

namespace seq {
namespace ir {

const char Value::NodeId = 0;

Value *Value::clone() const {
  if (hasReplacement())
    return getActual()->clone();

  auto *res = doClone();
  for (auto it = attributes_begin(); it != attributes_end(); ++it) {
    auto *attr = getAttribute(*it);
    if (attr->needsClone())
      res->setAttribute(attr->clone(), *it);
  }
  return res;
}

} // namespace ir
} // namespace seq
