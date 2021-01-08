#include "value.h"

#include "module.h"

namespace seq {
namespace ir {

const char Value::NodeId = 0;

const char ValueProxy::NodeId = 0;

Value *ValueProxy::doClone() const {
  return getModule()->Nrs<ValueProxy>(getSrcInfo(), val, getName());
}

} // namespace ir
} // namespace seq
