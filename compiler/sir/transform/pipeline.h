#pragma once

#include "pass.h"
#include "sir/sir.h"

namespace seq {
namespace ir {
namespace transform {
namespace pipeline {

class PipelineOptimizations : public LambdaValuePass {
  void handle(PipelineFlow *x) override;
};

} // namespace pipeline
} // namespace transform
} // namespace ir
} // namespace seq
