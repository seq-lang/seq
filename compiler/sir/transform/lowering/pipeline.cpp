#include "pipeline.h"

#include <algorithm>

#include "sir/util/cloning.h"
#include "sir/util/irtools.h"
#include "sir/util/matching.h"

namespace seq {
namespace ir {
namespace transform {
namespace lowering {
namespace {
Value *convertPipelineToForLoopsHelper(Module *M, BodiedFunc *parent,
                                       const std::vector<PipelineFlow::Stage *> &stages,
                                       unsigned idx = 0, Value *last = nullptr) {
  if (idx >= stages.size())
    return last;
  auto *stage = stages[idx];
  Value *next = nullptr;

  if (last) {
    std::vector<Value *> args;
    for (auto *arg : *stage) {
      args.push_back(arg ? arg : last);
    }
    next = M->Nr<CallInstr>(stage->getCallee(), args);
  } else {
    next = stage->getCallee();
  }

  if (stage->isGenerator()) {
    auto *var = M->Nr<Var>(stage->getOutputElementType());
    parent->push_back(var);
    auto *body = convertPipelineToForLoopsHelper(M, parent, stages, idx + 1,
                                                 M->Nr<VarValue>(var));
    return M->Nr<ForFlow>(next, util::series(body), var, stage->isParallel());
  } else {
    return convertPipelineToForLoopsHelper(M, parent, stages, idx + 1, next);
  }
}

Value *convertPipelineToForLoops(PipelineFlow *p, BodiedFunc *parent) {
  std::vector<PipelineFlow::Stage *> stages;
  for (auto &stage : *p) {
    stages.push_back(&stage);
  }
  return convertPipelineToForLoopsHelper(p->getModule(), parent, stages);
}
} // namespace

const std::string PipelineLowering::KEY = "core-pipeline-lowering";

void PipelineLowering::handle(PipelineFlow *v) {
  v->replaceAll(convertPipelineToForLoops(v, cast<BodiedFunc>(getParentFunc())));
}

} // namespace lowering
} // namespace transform
} // namespace ir
} // namespace seq
