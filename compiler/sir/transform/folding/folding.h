#pragma once

#include "sir/transform/pass.h"

#include "sir/transform/cleanup/dead_code.h"

namespace seq {
namespace ir {
namespace transform {
namespace folding {

class FoldingPass;

/// Group of constant folding passes.
class FoldingPassGroup : public PassGroup {
private:
  FoldingPass *fp;
  cleanup::DeadCodeCleanupPass *dce;

public:
  const std::string KEY = "core-folding-pass-group";
  std::string getKey() const override { return KEY; }

  /// @param reachingDefPass the key of the reaching definitions pass
  explicit FoldingPassGroup(std::string reachingDefPass);

  bool shouldRepeat() const override;
};

} // namespace folding
} // namespace transform
} // namespace ir
} // namespace seq
