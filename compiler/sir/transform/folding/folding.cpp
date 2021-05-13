#include "folding.h"

#include "const_fold.h"
#include "const_prop.h"
#include "sir/transform/cleanup/dead_code.h"

namespace seq {
namespace ir {
namespace transform {
namespace folding {

FoldingPassGroup::FoldingPassGroup(std::string reachingDefPass) {
  push_back(std::make_unique<ConstPropPass>(std::move(reachingDefPass)));
  fp = new FoldingPass();
  push_back(std::unique_ptr<FoldingPass>(fp));
  dce = new cleanup::DeadCodeCleanupPass();
  push_back(std::unique_ptr<cleanup::DeadCodeCleanupPass>(dce));
}

bool FoldingPassGroup::shouldRepeat() const {
  return fp->getNumReplacements() != 0 || dce->getNumReplacements() != 0;
}

} // namespace folding
} // namespace transform
} // namespace ir
} // namespace seq
