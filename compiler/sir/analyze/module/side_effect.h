#pragma once

#include <unordered_map>

#include "sir/analyze/analysis.h"

namespace seq {
namespace ir {
namespace analyze {
namespace module {

struct SideEffectResult : public Result {
  /// mapping of ID to bool indicating whether the node has side effects
  std::unordered_map<id_t, bool> result;
  /// mapping of variable ID to number of times it is used in the module
  std::unordered_map<id_t, long> varCounts;
  /// mapping of variable ID to number of times it is assigned in the module
  std::unordered_map<id_t, long> varAssignCounts;

  SideEffectResult(std::unordered_map<id_t, bool> result,
                   std::unordered_map<id_t, long> varCounts,
                   std::unordered_map<id_t, long> varAssignCounts)
      : result(std::move(result)), varCounts(std::move(varCounts)),
        varAssignCounts(std::move(varAssignCounts)) {}

  /// @param v the value to check
  /// @return true if the node has side effects (false positives allowed)
  bool hasSideEffect(Value *v) const;
  /// @param v the var to check
  /// @return the number of times the given variable is used
  long getUsageCount(Var *v) const;
  /// @param v the var to check
  /// @return true if this variable is only ever assigned
  bool isOnlyAssigned(Var *v) const;
};

class SideEffectAnalysis : public Analysis {
  static const std::string KEY;
  std::string getKey() const override { return KEY; }

  std::unique_ptr<Result> run(const Module *m) override;
};

} // namespace module
} // namespace analyze
} // namespace ir
} // namespace seq
