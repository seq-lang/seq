#include "manager.h"

namespace seq {
namespace ir {
namespace passes {

void PassManager::registerPass(std::unique_ptr<Pass> pass, const std::string &key,
                               std::vector<std::string> reqs,
                               std::vector<std::string> invalidates) {
  auto isAnalysis = pass->isAnalysis();
  passes.insert(std::make_pair(key, PassMetadata(std::move(pass), std::move(reqs),
                                                 std::move(invalidates), isAnalysis)));
  if (!isAnalysis)
    executionOrder.push_back(key);
}

void PassManager::run(IRModule *module) {
  for (auto &p : executionOrder) {
    runPass(module, p);
  }
}

void PassManager::runPass(IRModule *module, const std::string &name) {
  auto &meta = passes[name];

  auto done = false;
  while (!done) {
    done = true;
    for (auto &dep : meta.reqs) {
      if (valid.find(dep) == valid.end()) {
        runPass(module, dep);
        done = false;
      }
    }
  }

  meta.pass->run(module);

  valid.insert(name);
  for (auto &inv : meta.invalidates) {
    auto it = valid.find(inv);
    if (it != valid.end())
      valid.erase(it);
  }
}

} // namespace passes
} // namespace ir
} // namespace seq