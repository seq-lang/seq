#include "manager.h"

#include "pass.h"
#include "sir/transform/lowering/imperative.h"
#include "sir/transform/manager.h"
#include "sir/transform/pythonic/dict.h"
#include "sir/transform/pythonic/io.h"
#include "sir/transform/pythonic/str.h"
#include "util/common.h"

namespace seq {
namespace ir {
namespace transform {

std::string PassManager::KeyManager::getUniqueKey(const std::string &key) {
  // make sure we can't ever produce duplicate "unique'd" keys
  seqassert(key.find(':') == std::string::npos,
            "pass key '{}' contains invalid character ':'", key);
  auto it = keys.find(key);
  if (it == keys.end()) {
    keys.emplace(key, 1);
    return key;
  } else {
    auto id = ++(it->second);
    return key + ":" + std::to_string(id);
  }
}

std::string PassManager::registerPass(std::unique_ptr<Pass> pass,
                                      std::vector<std::string> reqs,
                                      std::vector<std::string> invalidates) {
  std::string key = pass->getKey();
  if (isDisabled(key))
    return "";
  key = km.getUniqueKey(key);
  passes.insert(std::make_pair(
      key, PassMetadata(std::move(pass), std::move(reqs), std::move(invalidates))));
  passes[key].pass->setManager(this);
  executionOrder.push_back(key);
  return key;
}

std::string PassManager::registerAnalysis(std::unique_ptr<analyze::Analysis> analysis) {
  std::string key = analysis->getKey();
  if (isDisabled(key))
    return "";
  key = km.getUniqueKey(key);
  analyses.insert(std::make_pair(key, std::move(analysis)));
  return key;
}

void PassManager::run(Module *module) {
  for (auto &p : executionOrder) {
    runPass(module, p);
  }
}

void PassManager::runPass(Module *module, const std::string &name) {
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

void PassManager::registerStandardPasses() {
  // Pythonic
  registerPass(std::make_unique<pythonic::DictArithmeticOptimization>());
  registerPass(std::make_unique<pythonic::StrAdditionOptimization>());
  registerPass(std::make_unique<pythonic::IOCatOptimization>());

  // lowering
  registerPass(std::make_unique<lowering::ImperativeForFlowLowering>());
}

} // namespace transform
} // namespace ir
} // namespace seq
