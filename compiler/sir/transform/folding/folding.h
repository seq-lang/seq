#pragma once

#include "sir/transform/pass.h"

#include "sir/transform/cleanup/canonical.h"
#include "sir/transform/cleanup/dead_code.h"

namespace seq {
namespace ir {
namespace transform {
namespace folding {

class FoldingPass;

/// Group of constant folding passes.
class FoldingPassGroup : public PassGroup {
private:
  cleanup::CanonicalizationPass *canon;
  FoldingPass *fp;
  cleanup::DeadCodeCleanupPass *dce;

public:
  const std::string KEY = "core-folding-pass-group";
  std::string getKey() const override { return KEY; }

  /// @param reachingDefPass the key of the reaching definitions pass
  /// @param globalVarPass the key of the global variables pass
  FoldingPassGroup(const std::string &reachingDefPass,
                   const std::string &globalVarPass);

  bool shouldRepeat() const override;
};

} // namespace folding
} // namespace transform
} // namespace ir
} // namespace seq
