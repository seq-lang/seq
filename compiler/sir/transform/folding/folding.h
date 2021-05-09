#pragma once

#include "const_fold.h"
#include "const_prop.h"

namespace seq {
namespace ir {
namespace transform {
namespace folding {

/// Group of constant folding passes.
class FoldingPassGroup : public PassGroup {
private:
  FoldingPass *fp;

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