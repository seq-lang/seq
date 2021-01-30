#pragma once

#include "sir/passes/pass.h"

namespace seq {
namespace ir {
namespace passes {
namespace cleanup {

class ReplaceCleanupPass : public IRTransformationPass {
public:
  void execute(IRModule *module) override;
};

} // namespace cleanup
} // namespace passes
} // namespace ir
} // namespace seq