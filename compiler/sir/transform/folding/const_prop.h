#pragma once

#include "sir/transform/pass.h"

namespace seq {
namespace ir {
namespace transform {
namespace folding {

/// Constant propagation pass.
class ConstPropPass : public OperatorPass {
private:
  /// Key of the reaching definition analysis
  std::string reachingDefKey;

public:
  const std::string KEY = "core-folding-const-prop";

  /// Constructs a constant propagation pass.
  /// @param reachingDefKey the reaching definition analysis' key
  explicit ConstPropPass(std::string reachingDefKey)
      : reachingDefKey(std::move(reachingDefKey)) {}

  std::string getKey() const override { return KEY; }
  void handle(VarValue *v) override;
};

} // namespace folding
} // namespace transform
} // namespace ir
} // namespace seq
