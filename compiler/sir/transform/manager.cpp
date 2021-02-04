#include "manager.h"

#include "pass.h"

namespace seq {
namespace ir {
namespace transform {

void PassManager::registerPass(const std::string &key, std::unique_ptr<Pass> pass,
                               std::vector<std::string> reqs,
                               std::vector<std::string> invalidates) {
  passes.insert(std::make_pair(
      key, PassMetadata(std::move(pass), std::move(reqs), std::move(invalidates))));
  passes[key].pass->setManager(this);
  executionOrder.push_back(key);
}

void PassManager::registerAnalysis(const std::string &key,
                                   std::unique_ptr<analyze::Analysis> analysis) {
  analyses.insert(std::make_pair(key, std::move(analysis)));
}

void PassManager::run(IRModule *module) {
  for (auto &p : executionOrder) {
    runPass(module, p);
  }
}

void PassManager::runPass(IRModule *module, const std::string &name) {
  auto &meta = passes[name];

  for (auto &dep : meta.reqs) {
    auto it = results.find(dep);
    if (it == results.end())
      results[dep] = analyses[dep]->run(module);
  }

  meta.pass->run(module);

  for (auto &inv : meta.invalidates)
    results.erase(inv);
}

} // namespace transform
} // namespace ir
} // namespace seq
