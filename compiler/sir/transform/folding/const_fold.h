#pragma once

#include <memory>
#include <unordered_map>

#include "rule.h"

#include "sir/transform/pass.h"

namespace seq {
namespace ir {
namespace transform {
namespace folding {

class FoldingPass : public OperatorPass {
private:
  std::unordered_map<std::string, std::unique_ptr<FoldingRule>> rules;
  int numReplacements = 0;

public:
  /// Constructs a folding pass.
  FoldingPass() : OperatorPass(true) {}

  const std::string KEY = "core-folding-const-fold";
  std::string getKey() const override { return KEY; }

  void run(Module *m) override;

  void registerRule(const std::string &key, std::unique_ptr<FoldingRule> rule) {
    rules.emplace(std::make_pair(key, std::move(rule)));
  }

  void handle(CallInstr *v) override;

  /// @return the number of replacements
  int getNumReplacements() const { return numReplacements; }

private:
  void registerStandardRules(Module *m);
};

} // namespace folding
} // namespace transform
} // namespace ir
} // namespace seq
