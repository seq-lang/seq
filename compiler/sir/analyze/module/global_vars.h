#pragma once

#include <unordered_map>

#include "sir/analyze/analysis.h"

namespace seq {
namespace ir {
namespace analyze {
namespace module {

struct GlobalVarsResult : public Result {
  std::unordered_map<id_t, id_t> assignments;
  explicit GlobalVarsResult(std::unordered_map<id_t, id_t> assignments)
      : assignments(std::move(assignments)) {}
};

class GlobalVarsAnalyses : public Analysis {
  static const std::string KEY;
  std::string getKey() const override { return KEY; }

  std::unique_ptr<Result> run(const Module *m) override;
};

} // namespace module
} // namespace analyze
} // namespace ir
} // namespace seq
