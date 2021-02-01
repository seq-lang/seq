#include "constant.h"

namespace seq {
namespace ir {

const char Constant::NodeId = 0;

int Constant::doReplaceUsedType(const std::string &name, types::Type *newType) {
  if (type->getName() == name) {
    type = newType;
    return 1;
  }
  return 0;
}

const char TemplatedConstant<std::string>::NodeId = 0;

} // namespace ir
} // namespace seq
