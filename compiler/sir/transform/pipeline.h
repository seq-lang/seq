#pragma once

#include "pass.h"
#include "sir/sir.h"

namespace seq {
namespace ir {
namespace transform {
namespace pipeline {

class PipelineOptimizations : public OperatorPass {
  const unsigned SCHED_WIDTH_PREFETCH = 16;
  const unsigned SCHED_WIDTH_INTERALIGN = 2048;

  void handle(PipelineFlow *) override;

  void applySubstitutionOptimizations(PipelineFlow *);
  void applyPrefetchOptimizations(PipelineFlow *);
  void applyInterAlignOptimizations(PipelineFlow *);
};

} // namespace pipeline
} // namespace transform
} // namespace ir
} // namespace seq
