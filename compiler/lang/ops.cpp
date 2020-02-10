#include "lang/seq.h"

using namespace seq;

static std::vector<Op> ops;

static void init() {
  if (ops.empty()) {
    ops = {
        {"~", false, "__invert__"},
        {"!", false},
        {"-", false, "__neg__"},
        {"+", false, "__pos__"},
        {"**", true, "__pow__", "__rpow__", "__ipow__"},
        {"*", true, "__mul__", "__rmul__", "__imul__"},
        {"@", true, "__matmul__", "__rmatmul__", "__imatmul__"},
        {"//", true, "__div__", "__rdiv__", "__idiv__"},
        {"/", true, "__truediv__", "__rtruediv__", "__itruediv__"},
        {"%", true, "__mod__", "__rmod__", "__imod__"},
        {"+", true, "__add__", "__radd__", "__iadd__"},
        {"-", true, "__sub__", "__rsub__", "__isub__"},
        {"<<", true, "__lshift__", "__rlshift__", "__ilshift__"},
        {">>", true, "__rshift__", "__rrshift__", "__irshift__"},
        {"<", true, "__lt__"},
        {">", true, "__gt__"},
        {"<=", true, "__le__"},
        {">=", true, "__ge__"},
        {"==", true, "__eq__"},
        {"!=", true, "__ne__"},
        {"&", true, "__and__", "__rand__", "__iand__"},
        {"^", true, "__xor__", "__rxor__", "__ixor__"},
        {"|", true, "__or__", "__ror__", "__ior__"},
        {"&&", true},
        {"||", true},
    };
  }
}

Op seq::uop(const std::string &symbol) {
  init();

  for (auto &op : ops) {
    if (!op.binary && op.symbol == symbol)
      return op;
  }

  assert(0);
  return ops[0];
}

Op seq::bop(const std::string &symbol) {
  init();

  for (auto &op : ops) {
    if (op.binary && op.symbol == symbol)
      return op;
  }

  assert(0);
  return ops[0];
}
