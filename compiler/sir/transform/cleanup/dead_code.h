#pragma once

#include "sir/transform/pass.h"

namespace seq {
namespace ir {
namespace transform {
namespace cleanup {

/// Cleanup pass that removes dead code.
class DeadCodeCleanupPass : public OperatorPass {
private:
  int numReplacements = 0;

public:
  const std::string KEY = "core-cleanup-dce";
  std::string getKey() const override { return KEY; }

  void run(Module *m) override;

  void handle(IfFlow *v) override;
  void handle(WhileFlow *v) override;
  void handle(ImperativeForFlow *v) override;
  void handle(TernaryInstr *v) override;

  /// @return the number of replacements
  int getNumReplacements() const { return numReplacements; }

private:
  void doReplacement(Value *og, Value *v);
};

} // namespace cleanup
} // namespace transform
} // namespace ir
} // namespace seq
