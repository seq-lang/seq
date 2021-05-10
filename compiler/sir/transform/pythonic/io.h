#pragma once

#include "sir/transform/pass.h"

namespace seq {
namespace ir {
namespace transform {
namespace pythonic {

/// Pass to optimize print str.cat(...) or file.write(str.cat(...)).
class IOCatOptimization : public OperatorPass {
public:
  const std::string KEY = "core-pythonic-io-cat-opt";
  std::string getKey() const override { return KEY; }
  void handle(CallInstr *v) override;
};

} // namespace pythonic
} // namespace transform
} // namespace ir
} // namespace seq
