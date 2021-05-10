#pragma once

#include "sir/sir.h"
#include "sir/transform/pass.h"

namespace seq {

class PipelineSubstitutionOptimization : public ir::transform::OperatorPass {
  const std::string KEY = "seq-pipeline-subst-opt";
  std::string getKey() const override { return KEY; }
  void handle(ir::PipelineFlow *) override;
};

class PipelinePrefetchOptimization : public ir::transform::OperatorPass {
  const unsigned SCHED_WIDTH_PREFETCH = 16;
  const std::string KEY = "seq-pipeline-prefetch-opt";
  std::string getKey() const override { return KEY; }
  void handle(ir::PipelineFlow *) override;
};

class PipelineInterAlignOptimization : public ir::transform::OperatorPass {
  const unsigned SCHED_WIDTH_INTERALIGN = 2048;
  const std::string KEY = "seq-pipeline-inter-align-opt";
  std::string getKey() const override { return KEY; }
  void handle(ir::PipelineFlow *) override;
};

} // namespace seq
