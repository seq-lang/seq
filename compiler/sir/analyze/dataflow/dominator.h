#pragma once

#include <set>
#include <unordered_map>
#include <utility>

#include "sir/analyze/analysis.h"
#include "sir/analyze/dataflow/cfg.h"

namespace seq {
namespace ir {
namespace analyze {
namespace dataflow {

/// Helper to query the dominators of a particular function.
class DominatorInspector {
private:
  std::unordered_map<id_t, std::set<id_t>> sets;
  CFGraph *cfg;

public:
  explicit DominatorInspector(CFGraph *cfg) : cfg(cfg) {}

  /// Do the analysis.
  void analyze();

  /// Checks if one value dominates another.
  /// @param v the value
  /// @param dominator the dominator value
  bool isDominated(const Value *v, const Value *dominator);
};

/// Result of a dominator analysis.
struct DominatorResult : public Result {
  /// the corresponding control flow result
  const CFResult *cfgResult;
  /// the dominator inspectors
  std::unordered_map<id_t, std::unique_ptr<DominatorInspector>> results;

  explicit DominatorResult(const CFResult *cfgResult) : cfgResult(cfgResult) {}
};

/// Dominator analysis. Must have control flow-graph available.
class DominatorAnalysis : public Analysis {
private:
  /// the control-flow analysis key
  std::string cfAnalysisKey;

public:
  const std::string KEY = "core-analyses-dominator";

  /// Initializes a dominator analysis.
  /// @param cfAnalysisKey the control-flow analysis key
  explicit DominatorAnalysis(std::string cfAnalysisKey)
      : cfAnalysisKey(std::move(cfAnalysisKey)) {}

  std::string getKey() const override { return KEY; }

  std::unique_ptr<Result> run(const Module *m) override;
};

} // namespace dataflow
} // namespace analyze
} // namespace ir
} // namespace seq
