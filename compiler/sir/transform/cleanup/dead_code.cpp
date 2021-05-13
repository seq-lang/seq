#include "dead_code.h"

#include "sir/util/cloning.h"

namespace {
using namespace seq::ir;

BoolConst *boolConst(Value *v) { return cast<BoolConst>(v); }

IntConst *intConst(Value *v) { return cast<IntConst>(v); }

} // namespace

namespace seq {
namespace ir {
namespace transform {
namespace cleanup {

void DeadCodeCleanupPass::run(Module *m) {
  numReplacements = 0;
  OperatorPass::run(m);
}

void DeadCodeCleanupPass::handle(IfFlow *v) {
  auto *cond = boolConst(v->getCond());
  if (!cond)
    return;

  auto *M = v->getModule();
  auto condVal = cond->getVal();

  util::CloneVisitor cv(M);
  if (condVal) {
    doReplacement(v, cv.clone(v->getTrueBranch()));
  } else if (auto *f = v->getFalseBranch()) {
    doReplacement(v, cv.clone(f));
  } else {
    doReplacement(v, M->Nr<SeriesFlow>());
  }
}

void DeadCodeCleanupPass::handle(WhileFlow *v) {
  auto *cond = boolConst(v->getCond());
  if (!cond)
    return;

  auto *M = v->getModule();
  auto condVal = cond->getVal();
  if (!condVal) {
    doReplacement(v, M->Nr<SeriesFlow>());
  }
}

void DeadCodeCleanupPass::handle(ImperativeForFlow *v) {
  auto *start = intConst(v->getStart());
  auto *end = intConst(v->getEnd());
  if (!start || !end)
    return;

  auto stepVal = v->getStep();
  auto startVal = start->getVal();
  auto endVal = end->getVal();

  auto *M = v->getModule();
  if ((stepVal < 0 && startVal <= endVal) || (stepVal > 0 && startVal >= endVal)) {
    doReplacement(v, M->Nr<SeriesFlow>());
  }
}

void DeadCodeCleanupPass::handle(TernaryInstr *v) {
  auto *cond = boolConst(v->getCond());
  if (!cond)
    return;

  auto *M = v->getModule();
  auto condVal = cond->getVal();

  util::CloneVisitor cv(M);
  if (condVal) {
    doReplacement(v, cv.clone(v->getTrueValue()));
  } else {
    doReplacement(v, cv.clone(v->getFalseValue()));
  }
}

void DeadCodeCleanupPass::doReplacement(Value *og, Value *v) {
  numReplacements++;
  og->replaceAll(v);
}

} // namespace cleanup
} // namespace transform
} // namespace ir
} // namespace seq
