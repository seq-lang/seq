#include "seq.h"
#include "pipeline.h"
#include "revcomp.h"

namespace seq {

void Seq::addIRPasses(ir::transform::PassManager *pm, bool debug) {
  pm->registerPass(std::make_unique<KmerRevcompInterceptor>());
  if (debug)
    return;
  pm->registerPass(std::make_unique<PipelineSubstitutionOptimization>());
  pm->registerPass(std::make_unique<PipelinePrefetchOptimization>());
  pm->registerPass(std::make_unique<PipelineInterAlignOptimization>());
}

} // namespace seq
