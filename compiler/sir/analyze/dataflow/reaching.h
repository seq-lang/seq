#pragma once

#include <utility>

#include "sir/analyze/analysis.h"
#include "sir/analyze/dataflow/cfg.h"

namespace seq {
namespace ir {
namespace analyze {
namespace dataflow {

class RDInspector {
private:
  struct BlockData {
    std::unordered_map<int, std::unordered_set<int>> in;
    std::unordered_map<int, std::unordered_set<int>> out;
    std::unordered_set<int> killed;
    std::unordered_map<int, std::unordered_set<int>> generated;

    BlockData() = default;
  };
  std::unordered_map<int, BlockData> sets;

public:
  void analyze(CFGraph *cfg);

private:
  void initializeIfNecessary(CFBlock *blk);

  void calculateIn(CFBlock *blk);
  bool calculateOut(CFBlock *blk);
};

/// Reaching definition analysis. Must have control flow-graph available.
class RDAnalysis : public Analysis {
private:
  /// the control-flow analysis key
  std::string cfAnalysisKey;

public:
  /// Initializes a reaching definition analysis.
  /// @param cfAnalysisKey the control-flow analysis key
  explicit RDAnalysis(std::string cfAnalysisKey)
      : cfAnalysisKey(std::move(cfAnalysisKey)) {}

  std::unique_ptr<Result> run(const Module *m) override;
};

} // namespace dataflow
} // namespace analyze
} // namespace ir
} // namespace seq
