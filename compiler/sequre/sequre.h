#pragma once

#include "dsl/dsl.h"

namespace seq {

class Sequre : public DSL {
public:
  std::string getName() const override { return "Sequre"; }
  void addIRPasses(ir::transform::PassManager *pm, bool debug) override;
};

} // namespace seq
