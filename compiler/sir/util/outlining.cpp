#include "outlining.h"

#include <iterator>
#include <unordered_set>
#include <utility>

#include "cloning.h"
#include "irtools.h"
#include "operator.h"

namespace seq {
namespace ir {
namespace util {
namespace {
struct OutlineReplacer : public Operator {
  std::vector<std::pair<Var *, Var *>> &remap;
  std::vector<Value *> &outFlows;
  CloneVisitor cv;

  OutlineReplacer(Module *M, std::vector<std::pair<Var *, Var *>> &remap,
                  std::vector<Value *> &outFlows)
      : Operator(), remap(remap), outFlows(outFlows), cv(M) {}

  void postHook(Node *node) override {
    for (auto &pair : remap) {
      node->replaceUsedVariable(std::get<0>(pair), std::get<1>(pair));
    }
  }

  template <typename InstrType> void replaceOutFlowWithReturn(InstrType *v) {
    auto *M = v->getModule();
    for (unsigned i = 0; i < outFlows.size(); i++) {
      if (outFlows[i]->getId() == v->getId()) {
        auto *copy = cv.clone(v);
        v->replaceAll(M->template Nr<ReturnInstr>(M->getInt(i)));
        outFlows[i] = copy;
        break;
      }
    }
  }

  void handle(ReturnInstr *v) override { replaceOutFlowWithReturn(v); }

  void handle(BreakInstr *v) override { replaceOutFlowWithReturn(v); }

  void handle(ContinueInstr *v) override { replaceOutFlowWithReturn(v); }
};

struct Outliner : public Operator {
  SeriesFlow *flowRegion;
  decltype(flowRegion->begin()) begin, end;
  bool inRegion;
  bool invalid;
  std::unordered_set<id_t> inVars;
  std::unordered_set<id_t> outVars;
  std::unordered_set<id_t> inLoops;
  std::vector<id_t> loops;
  std::vector<Value *> outFlows;

  Outliner(SeriesFlow *flowRegion, decltype(flowRegion->begin()) begin,
           decltype(flowRegion->begin()) end)
      : Operator(), flowRegion(flowRegion), begin(begin), end(end), inRegion(false),
        invalid(false), inVars(), outVars(), inLoops(), loops(), outFlows() {}

  bool isEnclosingLoopInRegion() {
    int d = depth();
    for (int i = 0; i < d; i++) {
      Flow *v = getParent<WhileFlow>(i);
      if (!v)
        v = getParent<ForFlow>(i);
      if (!v)
        v = getParent<ImperativeForFlow>(i);

      if (v)
        return inLoops.count(v->getId()) > 0;
    }
    return false;
  }

  void handle(WhileFlow *v) override {
    if (inRegion)
      inLoops.insert(v->getId());
  }

  void handle(ForFlow *v) override {
    if (inRegion)
      inLoops.insert(v->getId());
  }

  void handle(ImperativeForFlow *v) override {
    if (inRegion)
      inLoops.insert(v->getId());
  }

  void handle(ReturnInstr *v) override {
    if (inRegion)
      outFlows.push_back(v);
  }

  void handle(BreakInstr *v) override {
    if (inRegion && !isEnclosingLoopInRegion())
      outFlows.push_back(v);
  }

  void handle(ContinueInstr *v) override {
    if (inRegion && !isEnclosingLoopInRegion())
      outFlows.push_back(v);
  }

  void handle(YieldInstr *v) override {
    if (inRegion)
      invalid = true;
  }

  void handle(YieldInInstr *v) override {
    if (inRegion)
      invalid = true;
  }

  void visit(SeriesFlow *v) override {
    if (v->getId() != flowRegion->getId())
      return Operator::visit(v);

    auto it = flowRegion->begin();
    for (; it != begin; ++it) {
      (*it)->accept(*this);
    }

    inRegion = true;

    for (; it != end; ++it) {
      (*it)->accept(*this);
    }

    inRegion = false;

    for (; it != flowRegion->end(); ++it) {
      (*it)->accept(*this);
    }
  }

  void preHook(Node *node) override {
    auto vars = node->getUsedVariables();
    auto &set = (inRegion ? inVars : outVars);
    for (auto *var : vars) {
      if (!var->isGlobal())
        set.insert(var->getId());
    }
  }

  // private = used in region AND NOT used outside region
  std::unordered_set<id_t> getPrivateVars() {
    std::unordered_set<id_t> privateVars;
    for (auto id : inVars) {
      if (outVars.count(id) == 0)
        privateVars.insert(id);
    }
    return privateVars;
  }

  // shared = used in region AND used outside region
  std::unordered_set<id_t> getSharedVars() {
    std::unordered_set<id_t> sharedVars;
    for (auto id : inVars) {
      if (outVars.count(id) > 0)
        sharedVars.insert(id);
    }
    return sharedVars;
  }

  OutlineResult outline() {
    if (invalid)
      return {};

    auto *M = flowRegion->getModule();
    std::vector<std::pair<Var *, Var *>> remap;
    std::vector<types::Type *> argTypes;
    std::vector<std::string> argNames;

    unsigned idx = 0;
    auto shared = getSharedVars();
    for (auto id : shared) {
      Var *var = M->getVar(id);
      seqassert(var, "unknown var id");
      remap.emplace_back(var, nullptr);
      argTypes.push_back(var->getType());
      argNames.push_back(std::to_string(idx++));
    }

    const bool callIndicatesControl = !outFlows.empty();
    auto *funcType = M->getFuncType(
        callIndicatesControl ? M->getIntType() : M->getVoidType(), argTypes);
    auto *outlinedFunc = M->Nr<BodiedFunc>("__outlined");
    outlinedFunc->realize(funcType, argNames);

    idx = 0;
    for (auto it = outlinedFunc->arg_begin(); it != outlinedFunc->arg_end(); ++it) {
      remap[idx] = {std::get<0>(remap[idx]), *it};
      ++idx;
    }

    for (auto id : getPrivateVars()) {
      Var *var = M->getVar(id);
      seqassert(var, "unknown var id");
      Var *newVar = M->N<Var>(var->getSrcInfo(), var->getType(), /*global=*/false,
                              var->getName());
      remap.emplace_back(var, newVar);
      outlinedFunc->push_back(newVar);
    }

    auto *body = M->N<SeriesFlow>((*begin)->getSrcInfo());
    auto it = begin;
    while (it != end) {
      body->push_back(*it);
      it = flowRegion->erase(it);
    }
    outlinedFunc->setBody(body);

    OutlineReplacer outRep(M, remap, outFlows);
    body->accept(outRep);

    std::vector<Value *> args;
    for (auto id : shared) {
      args.push_back(M->Nr<VarValue>(M->getVar(id)));
    }
    auto *outlinedCall = call(outlinedFunc, args);

    it = flowRegion->insert(it, outlinedCall);
    if (callIndicatesControl) {
      for (unsigned i = 0; i < outFlows.size(); i++) {
        auto *codeVal = M->getInt(i);
        auto *codeCheck = (*codeVal == *outlinedCall);
        auto *codeBody = series(outFlows[i]);
        auto *codeIf = M->Nr<IfFlow>(codeCheck, codeBody);
        ++it;
        it = flowRegion->insert(it, codeIf);
      }
    }

    return {outlinedFunc, outlinedCall, callIndicatesControl};
  }
};

} // namespace

OutlineResult outlineRegion(Func *func, SeriesFlow *series,
                            decltype(series->begin()) begin,
                            decltype(series->end()) end) {
  Outliner outliner(series, begin, end);
  func->accept(outliner);
  return outliner.outline();
}

} // namespace util
} // namespace ir
} // namespace seq
