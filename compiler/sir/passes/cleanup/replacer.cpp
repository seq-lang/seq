#include "replacer.h"

#include <unordered_set>

namespace seq {
namespace ir {
namespace passes {
namespace cleanup {

void ReplaceCleanupPass::execute(IRModule *module) {
  std::unordered_set<Value *> toDelete;

  for (auto it = module->values_begin(); it != module->values_end(); ++it) {
    auto oldChildren = it->getChildren();
    for (auto *c : oldChildren) {
      if (c->isReplaced()) {
        it->replaceChild(c, c->getActual());
        toDelete.insert(c);
      }
    }
  }

  for (auto *v : toDelete)
    module->remove(v);
}

} // namespace cleanup
} // namespace passes
} // namespace ir
} // namespace seq