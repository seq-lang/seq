#pragma once

#include "sir/transform/pass.h"
#include "sir/transform/rewrite.h"

namespace seq {
namespace ir {
namespace transform {
namespace cleanup {

/// Canonicalization pass that flattens nested series
/// flows, puts operands in a predefined order, etc.
class CanonicalizationPass : public OperatorPass, public Rewriter {
public:
  /// Constructs a canonicalization pass
  CanonicalizationPass() : OperatorPass(/*childrenFirst=*/true) {}

  const std::string KEY = "core-cleanup-canon";
  std::string getKey() const override { return KEY; }

  void run(Module *m) override;
  void handle(CallInstr *) override;
  void handle(SeriesFlow *) override;

private:
  void registerStandardRules(Module *m);
};

} // namespace cleanup
} // namespace transform
} // namespace ir
} // namespace seq
