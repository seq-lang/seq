#pragma once

#include "sir/transform/pass.h"

namespace seq {
namespace ir {
namespace transform {
namespace pythonic {

/// Pass to optimize calls of form d[x] = func(d[x], any).
/// This will work on any dictionary-like object that implements _do_op and
/// _do_op_throws as well as getters.
class DictArithmeticOptimization : public OperatorPass {
public:
  const std::string KEY = "core-pythonic-dict-arithmetic-opt";
  std::string getKey() const override { return KEY; }
  void handle(CallInstr *v) override;
};

} // namespace pythonic
} // namespace transform
} // namespace ir
} // namespace seq
