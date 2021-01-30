#pragma once

#include "sir/sir.h"

namespace seq {
namespace ir {
namespace passes {

class IRTransformationPass {
public:
  virtual void execute(IRModule *module) = 0;
};

class IRAnalysisPass {
public:
  virtual void execute(const IRModule *module) = 0;
};

} // namespace passes
} // namespace ir
} // namespace seq