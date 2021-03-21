#pragma once

#include "sir/transform/pass.h"

namespace seq {
namespace ir {
namespace transform {
namespace cleanup {

/// Cleanup pass that physically replaces nodes.
class ReplaceCleanupPass : public Pass {
public:
  void run(Module *module) override;
};

} // namespace cleanup
} // namespace transform
} // namespace ir
} // namespace seq
