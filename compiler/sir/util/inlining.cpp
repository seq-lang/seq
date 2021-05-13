#include "inlining.h"

#include <algorithm>

#include "sir/util/cloning.h"
#include "sir/util/irtools.h"
#include "sir/util/operator.h"

namespace {
using namespace seq::ir;

class ReturnReplacer : public util::Operator {
private:
  Value *implicitLoop;
  Var *var;
  util::CloneVisitor &cv;

public:
  ReturnReplacer(Value *implicitLoop, Var *var, util::CloneVisitor &cv)
      : implicitLoop(implicitLoop), var(var), cv(cv) {}

  void handle(ReturnInstr *v) {
    auto *M = v->getModule();
    auto *rep = M->N<SeriesFlow>(v);
    if (var) {
      rep->push_back(M->N<AssignInstr>(v, var, cv.clone(v->getValue())));
    }
    rep->push_back(M->N<BreakInstr>(v, implicitLoop));
    v->replaceAll(rep);
  }
};

} // namespace

namespace seq {
namespace ir {
namespace util {

InlineResult inlineFunction(Func *func, std::vector<Value *> args, seq::SrcInfo info) {
  auto *bodied = cast<BodiedFunc>(func);
  auto *fType = cast<types::FuncType>(bodied->getType());
  if (!bodied || args.size() != std::distance(bodied->arg_begin(), bodied->arg_end()))
    return {false, nullptr, {}};
  auto *M = bodied->getModule();

  util::CloneVisitor cv(M);
  auto *newFlow = M->N<SeriesFlow>(info, bodied->getName() + "_inlined");

  std::vector<Var *> newVars;
  auto arg_it = bodied->arg_begin();
  for (auto i = 0; i < args.size(); ++i) {
    newVars.push_back(cv.forceClone(*arg_it++));
    newFlow->push_back(M->N<AssignInstr>(info, newVars.back(), cv.clone(args[i])));
  }
  for (auto *v : *bodied) {
    newVars.push_back(cv.forceClone(v));
  }
  Var *retVal = nullptr;
  if (fType->getReturnType()->getName() != M->getVoidType()->getName()) {
    retVal = M->N<Var>(info, fType->getReturnType());
    newVars.push_back(retVal);
  }

  auto *clonedBody = cv.clone(bodied->getBody());
  auto *implicitLoop = M->N<WhileFlow>(info, M->getBool(true), clonedBody);
  cast<SeriesFlow>(clonedBody)->push_back(M->N<BreakInstr>(info, implicitLoop));
  ReturnReplacer rr(implicitLoop, retVal, cv);
  rr.process(clonedBody);

  Value *v = implicitLoop;
  if (retVal) {
    v = M->N<FlowInstr>(info, implicitLoop, M->N<VarValue>(info, retVal));
  }
  return {true, v, std::move(newVars)};
}

InlineResult inlineCall(CallInstr *v) {
  return inlineFunction(util::getFunc(v->getCallee()),
                        std::vector<Value *>(v->begin(), v->end()), v->getSrcInfo());
}

} // namespace util
} // namespace ir
} // namespace seq
