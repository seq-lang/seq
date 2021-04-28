#include "imperative.h"

#include <algorithm>

#include "sir/util/cloning.h"
#include "sir/util/irtools.h"
#include "sir/util/matching.h"

namespace seq {
namespace ir {
namespace transform {
namespace lowering {

void ImperativeForFlowLowering::handle(ForFlow *v) {
  auto *M = v->getModule();

  auto *iterCall = cast<CallInstr>(v->getIter());
  if (!iterCall)
    return;

  auto *iterFunc = util::getFunc(iterCall->getCallee());
  if (!iterFunc || iterFunc->getUnmangledName() != Module::ITER_MAGIC_NAME)
    return;

  auto *rangeCall = cast<CallInstr>(iterCall->front());
  if (!rangeCall)
    return;

  auto *newRangeFunc = util::getFunc(rangeCall->getCallee());
  if (!newRangeFunc || newRangeFunc->getUnmangledName() != Module::NEW_MAGIC_NAME)
    return;
  auto *parentType = newRangeFunc->getParentType();
  auto *rangeType = M->getOrRealizeType("range", {}, "std.internal.types.range");

  if (!parentType || !rangeType || parentType->getName() != rangeType->getName())
    return;

  auto it = rangeCall->begin();
  auto argCount = std::distance(it, rangeCall->end());

  util::CloneVisitor cv(M);

  IntConst *stepConst;
  Value *start;
  Value *end;
  int64_t step = 0;

  switch (argCount) {
  case 1:
    start = M->getInt(0);
    end = cv.clone(*it);
    step = 1;
    break;
  case 2:
    start = cv.clone(*it++);
    end = cv.clone(*it);
    step = 1;
    break;
  case 3:
    start = cv.clone(*it++);
    end = cv.clone(*it++);
    stepConst = cast<IntConst>(*it);
    if (!stepConst)
      return;
    step = stepConst->getVal();
    break;
  default:
    seqassert(false, "unknown range constructor");
  }
  if (step == 0)
    return;

  v->replaceAll(M->N<ImperativeForFlow>(v->getSrcInfo(), start, step, end, v->getBody(),
                                        v->getVar()));
}

} // namespace lowering
} // namespace transform
} // namespace ir
} // namespace seq
