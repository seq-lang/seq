#include "inlining.h"

#include <algorithm>

#include "sir/util/cloning.h"
#include "sir/util/irtools.h"
#include "sir/util/operator.h"

namespace seq {
namespace ir {
namespace util {

namespace {
class ReturnReplacer : public util::Operator {
private:
  Value *implicitLoop;
  Var *var;
  bool agressive;
  util::CloneVisitor &cv;

public:
  bool ok = true;

  ReturnReplacer(Value *implicitLoop, Var *var, bool agressive, util::CloneVisitor &cv)
      : implicitLoop(implicitLoop), var(var), agressive(agressive), cv(cv) {}

  void handle(ReturnInstr *v) {
    if (!verifyReturn(v)) {
      ok = false;
      return;
    }

    auto *M = v->getModule();
    auto *rep = M->N<SeriesFlow>(v);
    if (var) {
      rep->push_back(M->N<AssignInstr>(v, var, cv.clone(v->getValue())));
    }
    if (agressive)
      rep->push_back(M->N<BreakInstr>(v, implicitLoop));

    v->replaceAll(rep);
  }

private:
  bool verifyReturn(ReturnInstr *v) {
    if (agressive)
      return true;

    auto it = parent_begin();
    if (it == parent_end())
      return false;

    SeriesFlow *prev = nullptr;
    while (it != parent_end()) {
      auto *cur = cast<SeriesFlow>(*it++);
      if (!cur || (prev && prev->back()->getId() != cur->getId()))
        return false;
      prev = cur;
    }
    return prev->back()->getId() == v->getId();
  }
};
} // namespace

InlineResult inlineFunction(Func *func, std::vector<Value *> args, bool agressive,
                            seq::SrcInfo info) {
  auto *bodied = cast<BodiedFunc>(func);
  if (!bodied)
    return {false, nullptr, {}};
  auto *fType = cast<types::FuncType>(bodied->getType());
  if (!fType || args.size() != std::distance(bodied->arg_begin(), bodied->arg_end()))
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

  WhileFlow *implicit = nullptr;
  if (agressive) {
    implicit = M->N<WhileFlow>(info, M->getBool(true), clonedBody);
    if (!retVal)
      cast<SeriesFlow>(clonedBody)->push_back(M->N<BreakInstr>(info, implicit));
  }

  ReturnReplacer rr(implicit, retVal, agressive, cv);
  rr.process(clonedBody);

  if (!rr.ok)
    return {false, nullptr, {}};

  Value *v = implicit ? implicit : clonedBody;
  if (retVal) {
    v = M->N<FlowInstr>(info, cast<Flow>(v), M->N<VarValue>(info, retVal));
  }
  return {true, v, std::move(newVars)};
}

InlineResult inlineCall(CallInstr *v, bool agressive) {
  return inlineFunction(util::getFunc(v->getCallee()),
                        std::vector<Value *>(v->begin(), v->end()), agressive,
                        v->getSrcInfo());
}

} // namespace util
} // namespace ir
} // namespace seq
