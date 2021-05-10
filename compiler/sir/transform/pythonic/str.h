#pragma once

#include "sir/transform/pass.h"

namespace seq {
namespace ir {
namespace transform {
namespace pythonic {

/// Pass to optimize str1 + str2 + ...
class StrAdditionOptimization : public OperatorPass {
public:
  const std::string KEY = "core-pythonic-str-addition-opt";
  std::string getKey() const override { return KEY; }
  void handle(CallInstr *v) override;
};

} // namespace pythonic
} // namespace transform
} // namespace ir
} // namespace seq
