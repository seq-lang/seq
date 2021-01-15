#include "value.h"

#include "module.h"

namespace seq {
namespace ir {

const char Value::NodeId = 0;

std::unique_ptr<Value> Value::clone() const {
  std::unique_ptr<Value> res(doClone());
  for (auto it = attributes_begin(); it != attributes_end(); ++it) {
    auto *attr = getAttribute(*it);
    if (attr->needsClone())
      res->setAttribute(attr->clone(), *it);
  }
  return res;
}

const char ValueProxy::NodeId = 0;

Value *ValueProxy::doClone() const {
  return getModule()->Nrs<ValueProxy>(getSrcInfo(), val, getName());
}

} // namespace ir
} // namespace seq
