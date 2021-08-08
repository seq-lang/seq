#pragma once

#include "sir/transform/pass.h"

namespace seq {
namespace ir {
namespace transform {
namespace lowering {

class PipelineLowering : public OperatorPass {
public:
  static const std::string KEY;
  std::string getKey() const override { return KEY; }
  void handle(PipelineFlow *v) override;
};

} // namespace lowering
} // namespace transform
} // namespace ir
} // namespace seq
