#include "lower_flows.h"

#include <algorithm>
#include <utility>

namespace seq {
namespace ir {
namespace transform {

using std::move;

SeriesFlow *LowerFlowsVisitor::Context::getInsertPoint() {
  auto f = getFrame();
  if (f->empty() || !isA<SeriesFlow>(f->back()))
    f->push_back(module->Nx<SeriesFlow>());
  return f->back()->as<SeriesFlow>();
}

void LowerFlowsVisitor::visit(IRModule *module) {
  ctx->module = module;

  for (auto *var : *module)
    process(var);

  process(module->getMainFunc());
}

void LowerFlowsVisitor::defaultVisit(Var *v) {}
void LowerFlowsVisitor::defaultVisit(Func *f) {}

void LowerFlowsVisitor::visit(BodiedFunc *f) {
  ctx->curFunc = f;

  auto newBody = ctx->module->Nxs<UnorderedFlow>(f->getBody()->getSrcInfo(), "body");
  ctx->pushFrame(newBody.get());
  process(f->getBody());
  ctx->popFrame();

  f->setBody(move(newBody));
}

void LowerFlowsVisitor::defaultVisit(Value *v) { result = v->clone(); }
void LowerFlowsVisitor::visit(ValueProxy *v) {
  result = v->clone();

  auto it = ctx->mappings.find(v->getValue()->getId());
  assert(it != ctx->mappings.end());
  cast<ValueProxy>(result)->setValue(it->second);
}

void LowerFlowsVisitor::visit(SeriesFlow *i) {
  for (auto *v : *i) {
    if (auto res = process(v)) {
      ctx->getInsertPoint()->push_back(std::move(res));
    }
  }
}

void LowerFlowsVisitor::visit(IfFlow *i) {
  auto cond = process(i->getCond());
  if (!cond)
    cond = i->getCond()->clone();

  auto namePrefix = i->getName().empty() ? fmt::format(FMT_STRING("if.{}"), i->getId())
                                         : i->referenceString();

  auto *trueFlow = ctx->module->Nrs<SeriesFlow>(
      i->getSrcInfo(), fmt::format(FMT_STRING("{}.true"), namePrefix));
  auto *falseFlow = ctx->module->Nrs<SeriesFlow>(
      i->getSrcInfo(), fmt::format(FMT_STRING("{}.false"), namePrefix));
  auto *doneFlow = ctx->module->Nrs<SeriesFlow>(
      i->getSrcInfo(), fmt::format(FMT_STRING("{}.done"), namePrefix));

  std::vector<CondBranchInstr::Target> targets;
  targets.emplace_back(makeProxy(trueFlow, i->getSrcInfo()), move(cond));

  if (i->getFalseBranch())
    targets.emplace_back(makeProxy(falseFlow, i->getSrcInfo()), nullptr);
  else
    targets.emplace_back(makeProxy(doneFlow, i->getSrcInfo()), nullptr);

  ctx->getInsertPoint()->push_back(
      ctx->module->Nxs<CondBranchInstr>(i->getSrcInfo(), move(targets)));

  ctx->getFrame()->push_back(FlowPtr(trueFlow));
  process(i->getTrueBranch());
  setTerminator(ctx->module->Nxs<BranchInstr>(i->getSrcInfo(),
                                              makeProxy(doneFlow, i->getSrcInfo())));

  if (i->getFalseBranch()) {
    ctx->getFrame()->push_back(FlowPtr(falseFlow));
    process(i->getFalseBranch());
    setTerminator(ctx->module->Nxs<BranchInstr>(i->getSrcInfo(),
                                                makeProxy(doneFlow, i->getSrcInfo())));
  }

  ctx->getFrame()->push_back(FlowPtr(doneFlow));
}

void LowerFlowsVisitor::visit(WhileFlow *i) {
  auto namePrefix = i->getName().empty()
                        ? fmt::format(FMT_STRING("while.{}"), i->getId())
                        : i->referenceString();

  auto *condFlow = ctx->module->Nrs<SeriesFlow>(
      i->getSrcInfo(), fmt::format(FMT_STRING("{}.cond"), namePrefix));
  auto *bodyFlow = ctx->module->Nrs<SeriesFlow>(
      i->getSrcInfo(), fmt::format(FMT_STRING("{}.body"), namePrefix));
  auto *doneFlow = ctx->module->Nrs<SeriesFlow>(
      i->getSrcInfo(), fmt::format(FMT_STRING("{}.done"), namePrefix));

  ctx->loopMappings[i->getId()] = {condFlow, doneFlow};

  setTerminator(ctx->module->Nxs<BranchInstr>(i->getSrcInfo(),
                                              makeProxy(condFlow, i->getSrcInfo())));

  ctx->getFrame()->push_back(FlowPtr(condFlow));
  auto cond = process(i->getCond());
  if (!cond)
    cond = i->getCond()->clone();

  std::vector<CondBranchInstr::Target> targets;
  targets.emplace_back(makeProxy(bodyFlow, i->getSrcInfo()), move(cond));
  targets.emplace_back(makeProxy(doneFlow, i->getSrcInfo()), nullptr);

  ctx->getInsertPoint()->push_back(
      ctx->module->Nxs<CondBranchInstr>(i->getSrcInfo(), move(targets)));

  ctx->getFrame()->push_back(FlowPtr(bodyFlow));
  process(i->getBody());
  setTerminator(ctx->module->Nxs<BranchInstr>(i->getSrcInfo(),
                                              makeProxy(condFlow, i->getSrcInfo())));

  ctx->getFrame()->push_back(FlowPtr(doneFlow));
}

void LowerFlowsVisitor::visit(ForFlow *i) {
  auto namePrefix = i->getName().empty() ? fmt::format(FMT_STRING("for.{}"), i->getId())
                                         : i->referenceString();

  auto *setupFlow = ctx->module->Nrs<SeriesFlow>(
      i->getSrcInfo(), fmt::format(FMT_STRING("{}.setup"), namePrefix));
  auto *condFlow = ctx->module->Nrs<SeriesFlow>(
      i->getSrcInfo(), fmt::format(FMT_STRING("{}.cond"), namePrefix));
  auto *bodyFlow = ctx->module->Nrs<SeriesFlow>(
      i->getSrcInfo(), fmt::format(FMT_STRING("{}.body"), namePrefix));
  auto *updateFlow = ctx->module->Nrs<SeriesFlow>(
      i->getSrcInfo(), fmt::format(FMT_STRING("{}.body"), namePrefix));
  auto *doneFlow = ctx->module->Nrs<SeriesFlow>(
      i->getSrcInfo(), fmt::format(FMT_STRING("{}.done"), namePrefix));

  ctx->loopMappings[i->getId()] = {updateFlow, doneFlow};

  setTerminator(ctx->module->Nxs<BranchInstr>(i->getSrcInfo(),
                                              makeProxy(setupFlow, i->getSrcInfo())));

  ctx->getFrame()->push_back(FlowPtr(setupFlow));

  auto iter = process(i->getIter());
  if (!iter)
    iter = i->getIter()->clone();

  ctx->mappings[iter->getId()] = iter.get();

  std::vector<ValuePtr> doneArgs;
  doneArgs.push_back(makeProxy(iter, i->getSrcInfo()));

  auto done = ctx->module->Nxs<CallInstr>(
      i->getSrcInfo(),
      ctx->module->Nxs<IntrinsicConstant>(i->getSrcInfo(), IntrinsicType::DONE,
                                          ctx->module->getBoolType()),
      move(doneArgs));

  std::vector<ValuePtr> nextArgs;
  nextArgs.push_back(makeProxy(iter, i->getSrcInfo()));

  auto next = ctx->module->Nxs<CallInstr>(
      i->getSrcInfo(),
      ctx->module->Nxs<IntrinsicConstant>(i->getSrcInfo(), IntrinsicType::NEXT,
                                          i->getVar()->getType()),
      move(nextArgs));

  ctx->getInsertPoint()->push_back(move(iter));

  std::vector<CondBranchInstr::Target> setupTargets;
  setupTargets.emplace_back(makeProxy(doneFlow, i->getSrcInfo()), done->clone());
  setupTargets.emplace_back(makeProxy(updateFlow, i->getSrcInfo()), nullptr);

  ctx->getInsertPoint()->push_back(
      ctx->module->Nxs<CondBranchInstr>(i->getSrcInfo(), move(setupTargets)));

  ctx->getFrame()->push_back(FlowPtr(condFlow));

  std::vector<CondBranchInstr::Target> condTargets;
  condTargets.emplace_back(makeProxy(doneFlow, i->getSrcInfo()), done->clone());
  condTargets.emplace_back(makeProxy(bodyFlow, i->getSrcInfo()), nullptr);

  ctx->getInsertPoint()->push_back(
      ctx->module->Nxs<CondBranchInstr>(i->getSrcInfo(), move(condTargets)));

  ctx->getFrame()->push_back(FlowPtr(updateFlow));

  auto undef = ctx->module->Nxs<UndefinedConstant>(i->getSrcInfo(), next->getType());
  auto rhs = process(ctx->module->Nxs<TernaryInstr>(i->getSrcInfo(), done->clone(),
                                                    move(undef), move(next)));
  ctx->getInsertPoint()->push_back(
      ctx->module->Nxs<AssignInstr>(i->getSrcInfo(), i->getVar(), move(rhs)));
  setTerminator(ctx->module->Nxs<BranchInstr>(i->getSrcInfo(),
                                              makeProxy(condFlow, i->getSrcInfo())));

  ctx->getFrame()->push_back(FlowPtr(bodyFlow));
  process(i->getBody());
  setTerminator(ctx->module->Nxs<BranchInstr>(i->getSrcInfo(),
                                              makeProxy(updateFlow, i->getSrcInfo())));

  ctx->getFrame()->push_back(FlowPtr(doneFlow));
}

void LowerFlowsVisitor::visit(TryCatchFlow *f) {
  auto namePrefix = f->getName().empty() ? fmt::format(FMT_STRING("tc.{}"), f->getId())
                                         : f->referenceString();

  auto newBody = ctx->module->Nxs<UnorderedFlow>(
      f->getSrcInfo(), fmt::format(FMT_STRING("{}.body"), namePrefix));
  setTerminator(ctx->module->Nxs<BranchInstr>(f->getSrcInfo(),
                                              makeProxy(newBody, f->getSrcInfo())));

  ctx->pushFrame(newBody.get());
  process(f->getBody());
  ctx->popFrame();

  std::unique_ptr<UnorderedFlow> newFinally;
  if (f->getFinally()) {
    newFinally = ctx->module->Nxs<UnorderedFlow>(
        f->getSrcInfo(), fmt::format(FMT_STRING("{}.finally"), namePrefix));
    ctx->pushFrame(newFinally.get());
    process(f->getFinally());
    ctx->popFrame();
  }

  auto newTc = ctx->module->Nxs<TryCatchFlow>(f->getSrcInfo(), move(newBody),
                                              move(newFinally), f->getName());

  for (auto &c : *f) {
    auto newHandler = ctx->module->Nxs<UnorderedFlow>(
        f->getSrcInfo(), fmt::format(FMT_STRING("{}.handler"), namePrefix));
    ctx->pushFrame(newBody.get());
    process(c.getHandler());
    ctx->popFrame();

    newTc->emplace_back(move(newHandler), c.getType(), c.getVar());
  }

  ctx->getFrame()->push_back(move(newTc));
}

void LowerFlowsVisitor::defaultVisit(Constant *c) { result = c->clone(); }

void LowerFlowsVisitor::defaultVisit(Instr *i) { result = i->clone(); }

void LowerFlowsVisitor::visit(AssignInstr *i) {
  result = i->clone();
  if (auto rhs = process(i->getRhs()))
    cast<AssignInstr>(result)->setRhs(move(rhs));
}

void LowerFlowsVisitor::visit(ExtractInstr *i) {
  result = i->clone();
  if (auto val = process(i->getVal()))
    cast<ExtractInstr>(result)->setVal(move(val));
}

void LowerFlowsVisitor::visit(InsertInstr *i) {
  result = i->clone();
  if (auto lhs = process(i->getLhs()))
    cast<InsertInstr>(result)->setLhs(move(lhs));
  if (auto rhs = process(i->getRhs()))
    cast<InsertInstr>(result)->setRhs(move(rhs));
}

void LowerFlowsVisitor::visit(CallInstr *i) {
  result = i->clone();
  if (auto func = process(i->getFunc(), true))
    cast<CallInstr>(result)->setFunc(move(func));

  auto newArgs = std::vector<ValuePtr>();
  for (auto *a : *i) {
    if (auto newArg = process(a))
      newArgs.push_back(move(newArg));
    else
      newArgs.push_back(a->clone());
  }
  cast<CallInstr>(result)->setArgs(move(newArgs));
}

void LowerFlowsVisitor::visit(StackAllocInstr *i) {
  result = i->clone();
  if (auto count = process(i->getCount()))
    cast<StackAllocInstr>(i)->setCount(move(count));
}

void LowerFlowsVisitor::visit(TernaryInstr *i) {
  auto cond = process(i->getCond(), true);
  if (!cond)
    cond = i->getCond()->clone();

  auto namePrefix = i->getName().empty()
                        ? fmt::format(FMT_STRING("ternary.{}"), i->getId())
                        : i->referenceString();

  auto trueFlow = ctx->module->Nrs<SeriesFlow>(
      i->getSrcInfo(), fmt::format(FMT_STRING("{}.true"), namePrefix));
  auto falseFlow = ctx->module->Nrs<SeriesFlow>(
      i->getSrcInfo(), fmt::format(FMT_STRING("{}.false"), namePrefix));
  auto doneFlow = ctx->module->Nxs<SeriesFlow>(
      i->getSrcInfo(), fmt::format(FMT_STRING("{}.done"), namePrefix));

  std::vector<CondBranchInstr::Target> targets;
  targets.emplace_back(makeProxy(trueFlow, i->getSrcInfo()), move(cond));
  targets.emplace_back(makeProxy(falseFlow, i->getSrcInfo()), move(cond));

  ctx->getInsertPoint()->push_back(
      ctx->module->Nxs<CondBranchInstr>(i->getSrcInfo(), move(targets)));

  ctx->getFrame()->push_back(FlowPtr(trueFlow));
  auto tVal = process(i->getTrueValue());
  if (!tVal)
    tVal = i->getTrueValue()->clone();
  setTerminator(ctx->module->Nxs<BranchInstr>(i->getSrcInfo(),
                                              makeProxy(doneFlow, i->getSrcInfo())));

  ctx->getFrame()->push_back(FlowPtr(falseFlow));
  auto fVal = process(i->getFalseValue());
  if (!fVal)
    fVal = i->getFalseValue()->clone();
  setTerminator(ctx->module->Nxs<BranchInstr>(i->getSrcInfo(),
                                              makeProxy(doneFlow, i->getSrcInfo())));

  ctx->getFrame()->push_back(move(doneFlow));

  std::vector<PhiInstr::Predecessor> preds;
  preds.emplace_back(makeProxy(trueFlow, i->getSrcInfo()), move(tVal));
  preds.emplace_back(makeProxy(falseFlow, i->getSrcInfo()), move(fVal));

  result = ctx->module->Nxs<PhiInstr>(i->getSrcInfo(), move(preds));
}

void LowerFlowsVisitor::visit(BreakInstr *i) {
  auto *target = i->getTarget();

  result = ctx->module->Nxs<BranchInstr>(
      i->getSrcInfo(), makeProxy(ctx->loopMappings[target->getId()].breakDst));
}

void LowerFlowsVisitor::visit(ContinueInstr *i) {
  auto *target = i->getTarget();

  result = ctx->module->Nxs<BranchInstr>(
      i->getSrcInfo(), makeProxy(ctx->loopMappings[target->getId()].continueDst));
}

void LowerFlowsVisitor::visit(ReturnInstr *i) {
  result = i->clone();
  if (i->getValue())
    if (auto val = process(i->getValue()))
      cast<ReturnInstr>(result)->setValue(move(val));
}

void LowerFlowsVisitor::visit(YieldInstr *i) {
  result = i->clone();
  if (i->getValue())
    if (auto val = process(i->getValue()))
      cast<YieldInstr>(result)->setValue(move(val));
}

void LowerFlowsVisitor::visit(ThrowInstr *i) {
  result = i->clone();
  if (i->getValue())
    if (auto val = process(i->getValue()))
      cast<ThrowInstr>(result)->setValue(move(val));
}

void LowerFlowsVisitor::visit(FlowInstr *i) {
  process(i->getFlow());
  auto val = process(i->getValue());
  result = val ? move(val) : i->getValue()->clone();
}

void LowerFlowsVisitor::process(IRModule *module) {
  LowerFlowsVisitor visitor;
  module->accept(visitor);
}

void LowerFlowsVisitor::process(Var *v) {
  LowerFlowsVisitor visitor(*this);
  v->accept(visitor);
}

ValuePtr LowerFlowsVisitor::process(Value *v, bool skipFlatten) {
  if (isA<Flow>(v)) {
    LowerFlowsVisitor visitor(*this);
    visitor.flatten = false;
    v->accept(visitor);
    return nullptr;
  }

  auto needsFlatten = (!skipFlatten && flatten) || needsUnroll(v);

  ValuePtr toInsert;
  if (needsFlatten) {
    toInsert = v->clone();
    v = toInsert.get();
  }

  LowerFlowsVisitor visitor(*this);
  visitor.flatten = needsFlatten;
  v->accept(visitor);

  if (toInsert) {
    if (visitor.result)
      toInsert = move(visitor.result);

    if (isA<ValueProxy>(toInsert))
      return toInsert;

    auto r = makeProxy(toInsert, toInsert->getSrcInfo());
    ctx->getInsertPoint()->push_back(move(toInsert));
    return r;
  }

  return move(visitor.result);
}

bool LowerFlowsVisitor::needsUnroll(Value *value) {
  std::vector<const Value *> open = {value};
  std::vector<const Value *> next = {};

  while (!open.empty()) {
    for (auto *v : open) {
      if (isA<TernaryInstr>(v) || isA<FlowInstr>(v))
        return true;
      auto children = v->getChildren();
      std::copy(children.begin(), children.end(), std::back_inserter(next));
    }
    open.swap(next);
    next.clear();
  }

  return false;
}

bool LowerFlowsVisitor::setTerminator(ValuePtr t) {
  if (!ctx->getInsertPoint()->empty() &&
      isA<ControlFlowInstr>(ctx->getInsertPoint()->back()))
    return false;
  ctx->getInsertPoint()->push_back(move(t));
  return true;
}

ValuePtr LowerFlowsVisitor::makeProxy(Value *other, SrcInfo s) {
  return ctx->module->Nxs<ValueProxy>(move(s), other);
}

} // namespace transform
} // namespace ir
} // namespace seq