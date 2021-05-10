#pragma once

#include "pass.h"
#include "sir/sir.h"

namespace seq {
namespace ir {
namespace transform {
namespace sequre {

class ArithmeticsOptimizations : public OperatorPass {
  const std::string KEY = "sequre-arithmetic-opt";
  std::string getKey() const override { return KEY; }

  void handle(CallInstr *) override;

  void applyBeaverOptimizations(CallInstr *);
  void applyPolynomialOptimizations(CallInstr *);
  void applyOptimizations(CallInstr *);
};

} // namespace sequre
} // namespace transform
} // namespace ir
} // namespace seq
