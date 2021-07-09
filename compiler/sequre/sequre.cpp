#include "sequre.h"
#include "arithmetics.h"

namespace seq {

void Sequre::addIRPasses(ir::transform::PassManager *pm, bool debug) {
  pm->registerPass(std::make_unique<ArithmeticsOptimizations>());
}

} // namespace seq
