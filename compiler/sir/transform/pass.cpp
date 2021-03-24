#include "pass.h"

#include "manager.h"

namespace seq {
namespace ir {
namespace transform {

const analyze::Result *Pass::doGetAnalysis(const std::string &key) {
  return manager ? manager->getAnalysisResult(key) : nullptr;
}

} // namespace transform
} // namespace ir
} // namespace seq
