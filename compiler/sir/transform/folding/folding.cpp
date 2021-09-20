#include "folding.h"

#include "const_fold.h"
#include "const_prop.h"

namespace seq {
namespace ir {
namespace transform {
namespace folding {

const std::string FoldingPassGroup::KEY = "core-folding-pass-group";

FoldingPassGroup::FoldingPassGroup(const std::string &sideEffectsPass,
                                   const std::string &reachingDefPass,
                                   const std::string &globalVarPass) {
  auto gdUnique = std::make_unique<cleanup::GlobalDemotionPass>();
  auto canonUnique = std::make_unique<cleanup::CanonicalizationPass>(sideEffectsPass);
  auto fpUnique = std::make_unique<FoldingPass>();
  auto dceUnique = std::make_unique<cleanup::DeadCodeCleanupPass>(sideEffectsPass);

  gd = gdUnique.get();
  canon = canonUnique.get();
  fp = fpUnique.get();
  dce = dceUnique.get();

  push_back(std::move(gdUnique));
  push_back(std::make_unique<ConstPropPass>(reachingDefPass, globalVarPass));
  push_back(std::move(canonUnique));
  push_back(std::move(fpUnique));
  push_back(std::move(dceUnique));
}

bool FoldingPassGroup::shouldRepeat() const {
  return gd->getNumDemotions() != 0 || canon->getNumReplacements() != 0 ||
         fp->getNumReplacements() != 0 || dce->getNumReplacements() != 0;
}

} // namespace folding
} // namespace transform
} // namespace ir
} // namespace seq
