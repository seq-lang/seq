#pragma once

#include "sir/transform/pass.h"

namespace seq {
namespace ir {
namespace transform {
namespace pythonic {

/// Pass to optimize str1 + str2 + ...
class StrAdditionTransform : public OperatorPass {
public:
  void handle(CallInstr *v) override;
};

} // namespace pythonic
} // namespace transform
} // namespace ir
} // namespace seq
