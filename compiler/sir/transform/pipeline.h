#pragma once

#include "pass.h"
#include "sir/sir.h"

namespace seq {
namespace ir {
namespace transform {
namespace pipeline {

class PipelineSubstitutionOptimization : public OperatorPass {
  const std::string KEY = "seq-pipeline-subst-opt";
  std::string getKey() const override { return KEY; }
  void handle(PipelineFlow *) override;
};

class PipelinePrefetchOptimization : public OperatorPass {
  const unsigned SCHED_WIDTH_PREFETCH = 16;
  const std::string KEY = "seq-pipeline-prefetch-opt";
  std::string getKey() const override { return KEY; }
  void handle(PipelineFlow *) override;
};

class PipelineInterAlignOptimization : public OperatorPass {
  const unsigned SCHED_WIDTH_INTERALIGN = 2048;
  const std::string KEY = "seq-pipeline-inter-align-opt";
  std::string getKey() const override { return KEY; }
  void handle(PipelineFlow *) override;
};

} // namespace pipeline
} // namespace transform
} // namespace ir
} // namespace seq
