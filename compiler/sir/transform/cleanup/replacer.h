#pragma once

#include "sir/transform/pass.h"

namespace seq {
namespace ir {
namespace transform {
namespace cleanup {

class ReplaceCleanupPass : public Pass {
public:
  void run(IRModule *module) override;
};

} // namespace cleanup
} // namespace transform
} // namespace ir
} // namespace seq
