#pragma once

#include "sir/passes/pass.h"

namespace seq {
namespace ir {
namespace passes {
namespace cleanup {

class ReplaceCleanupPass : public Pass {
public:
  void run(IRModule *module) override;
};

} // namespace cleanup
} // namespace passes
} // namespace ir
} // namespace seq