#pragma once

#include "sir/transform/pass.h"

namespace seq {
namespace ir {
namespace transform {
namespace parallel {

class OpenMPPass : public OperatorPass {
public:
  const std::string KEY = "core-parallel-openmp";
  std::string getKey() const override { return KEY; }

  void handle(ForFlow *) override;
  void handle(ImperativeForFlow *) override;
};

} // namespace parallel
} // namespace transform
} // namespace ir
} // namespace seq
