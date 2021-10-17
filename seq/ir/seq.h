#pragma once

#include "codon/dsl/dsl.h"

namespace seq {

class Seq : public codon::DSL {
public:
  void addIRPasses(codon::ir::transform::PassManager *pm, bool debug) override;
};

} // namespace seq
