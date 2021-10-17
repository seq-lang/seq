#include "seq.h"
#include "pipeline.h"
#include "revcomp.h"

#include "codon/sir/transform/lowering/pipeline.h"

namespace seq {

void Seq::addIRPasses(codon::ir::transform::PassManager *pm, bool debug) {
  pm->registerPass(std::make_unique<KmerRevcompInterceptor>());
  if (debug)
    return;
  auto dep = codon::ir::transform::lowering::PipelineLowering::KEY;
  pm->registerPass(std::make_unique<PipelineSubstitutionOptimization>(), dep);
  pm->registerPass(std::make_unique<PipelinePrefetchOptimization>(), dep);
  pm->registerPass(std::make_unique<PipelineInterAlignOptimization>(), dep);
}

} // namespace seq

extern "C" std::unique_ptr<codon::DSL> load() { return std::make_unique<seq::Seq>(); }
