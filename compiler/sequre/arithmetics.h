#pragma once

#include "sir/transform/pass.h"
#include "sir/sir.h"

namespace seq {

class ArithmeticsOptimizations : public ir::transform::OperatorPass {
  const std::string KEY = "sequre-arithmetic-opt";
  std::string getKey() const override { return KEY; }

  void handle(ir::CallInstr *) override;

  void applyBeaverOptimizations(ir::CallInstr *);
  void applyPolynomialOptimizations(ir::CallInstr *);
  void applyOptimizations(ir::CallInstr *);
};

} // namespace seq
