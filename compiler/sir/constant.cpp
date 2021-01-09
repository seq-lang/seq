#include "constant.h"

namespace seq {
namespace ir {

const char Constant::NodeId = 0;

const char TemplatedConstant<std::string>::NodeId = 0;

std::ostream &operator<<(std::ostream &os, const IntrinsicType &t) {
  switch (t) {
  case NEXT:
    os << "next";
    break;
  case DONE:
    os << "done";
    break;
  default:
    os << "???";
    break;
  }
  return os;
}

const char TemplatedConstant<IntrinsicType>::NodeId = 0;

const char UndefinedConstant::NodeId = 0;

} // namespace ir
} // namespace seq
