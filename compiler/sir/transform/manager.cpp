#include "manager.h"

#include <cassert>
#include <unordered_set>

#include "pass.h"
#include "sir/analyze/analysis.h"

namespace seq {
namespace ir {
namespace transform {

void PassManager::registerPass(const std::string &key, std::unique_ptr<Pass> pass,
                               std::vector<std::string> reqs,
                               std::vector<std::string> invalidates) {

  for (const auto &req : reqs) {
    assert(deps.find(req) != deps.end());
    deps[req].push_back(key);
  }

  passes.insert(std::make_pair(
      key, PassMetadata(std::move(pass), std::move(reqs), std::move(invalidates))));
  passes[key].pass->setManager(this);
  executionOrder.push_back(key);
  deps[key] = {};
}

void PassManager::registerAnalysis(const std::string &key,
                                   std::unique_ptr<analyze::Analysis> analysis,
                                   std::vector<std::string> reqs) {
  for (const auto &req : reqs) {
    assert(deps.find(req) != deps.end());
    deps[req].push_back(key);
  }

  analyses.insert(
      std::make_pair(key, AnalysisMetadata(std::move(analysis), std::move(reqs))));
  analyses[key].analysis->setManager(this);
  deps[key] = {};
}

void PassManager::run(IRModule *module) {
  for (auto &p : executionOrder) {
    runPass(module, p);
  }
}

void PassManager::runPass(IRModule *module, const std::string &name) {
  auto &meta = passes[name];

  for (auto &dep : meta.reqs) {
    runAnalysis(module, dep);
  }

  meta.pass->run(module);

  for (auto &inv : meta.invalidates)
    invalidate(inv);
}

void PassManager::runAnalysis(IRModule *module, const std::string &name) {
  if (results.find(name) != results.end())
    return;

  auto &meta = analyses[name];
  for (auto &dep : meta.reqs) {
    runAnalysis(module, dep);
  }
  results[name] = meta.analysis->run(module);
}

void PassManager::invalidate(const std::string &key) {
  std::unordered_set<std::string> open = {key};

  while (!open.empty()) {
    std::unordered_set<std::string> newOpen;
    for (const auto &k : open) {
      if (results.find(k) != results.end()) {
        results.erase(k);
        newOpen.insert(deps[k].begin(), deps[k].end());
      }
    }
    open = std::move(newOpen);
  }
}

} // namespace transform
} // namespace ir
} // namespace seq
