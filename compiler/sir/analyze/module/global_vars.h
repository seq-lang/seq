#pragma once

#include <unordered_map>

#include "sir/analyze/analysis.h"

namespace seq {
namespace ir {
namespace analyze {
namespace module {

struct GlobalVarsResult : public Result {
  std::unordered_map<id_t, id_t> assignments;
};

class GlobalVarsAnalyses : public Analysis  {
  const std::string KEY = "core-analyses-global-vars";
  std::string getKey() const override { return KEY; }

  std::unique_ptr<Result> run(const Module *m) override;
};

}
}
}
}
