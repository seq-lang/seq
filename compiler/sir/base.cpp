#include "base.h"

namespace seq {
namespace ir {

const char IRNode::NodeId = 0;

int IdMixin::currentId = 0;

void IdMixin::resetId() { currentId = 0; }

} // namespace ir
} // namespace seq
