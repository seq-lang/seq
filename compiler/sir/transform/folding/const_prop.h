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
  /// Key of the global variables analysis
  std::string globalVarsKey;

public:
  static const std::string KEY;

  /// Constructs a constant propagation pass.
  /// @param reachingDefKey the reaching definition analysis' key
  explicit ConstPropPass(std::string reachingDefKey, std::string globalVarsKey)
      : reachingDefKey(std::move(reachingDefKey)),
        globalVarsKey(std::move(globalVarsKey)) {}

  std::string getKey() const override { return KEY; }
  void handle(VarValue *v) override;
};

} // namespace folding
} // namespace transform
} // namespace ir
} // namespace seq
