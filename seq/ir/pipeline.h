#pragma once

#include "codon/sir/sir.h"
#include "codon/sir/transform/pass.h"

namespace seq {

class PipelineSubstitutionOptimization : public codon::ir::transform::OperatorPass {
  static const std::string KEY;
  std::string getKey() const override { return KEY; }
  void handle(codon::ir::PipelineFlow *) override;
};

class PipelinePrefetchOptimization : public codon::ir::transform::OperatorPass {
  const unsigned SCHED_WIDTH_PREFETCH = 16;
  static const std::string KEY;
  std::string getKey() const override { return KEY; }
  void handle(codon::ir::PipelineFlow *) override;
};

class PipelineInterAlignOptimization : public codon::ir::transform::OperatorPass {
  const unsigned SCHED_WIDTH_INTERALIGN = 2048;
  static const std::string KEY;
  std::string getKey() const override { return KEY; }
  void handle(codon::ir::PipelineFlow *) override;
};

} // namespace seq
