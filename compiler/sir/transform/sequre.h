#pragma once

#include "pass.h"
#include "sir/sir.h"

namespace seq {
namespace ir {
namespace transform {
namespace sequre {

class ArithmeticsOptimizations : public OperatorPass {
  void handle(CallInstr *) override;

  void applyBeaverOptimizations(CallInstr *, bool);
  void applyPolynomialOptimizations(CallInstr *);
  void applyOptimizations(CallInstr *);
};

} // namespace sequre
} // namespace transform
} // namespace ir
} // namespace seq