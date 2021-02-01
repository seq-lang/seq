#pragma once

#include "sir/sir.h"

namespace seq {
namespace ir {
namespace passes {

class Pass {
  virtual void run(IRModule *module) = 0;
};

class Analysis : public Pass {
public:
  void run(IRModule *module) override { runAnalysis(module); }
  virtual void runAnalysis(const IRModule *module) = 0;
};

} // namespace passes
} // namespace ir
} // namespace seq
