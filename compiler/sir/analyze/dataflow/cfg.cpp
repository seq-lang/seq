#include "cfg.h"

#include <vector>

#include "sir/dsl/codegen.h"
#include "sir/dsl/nodes.h"
#include "sir/util/visitor.h"

namespace {
using namespace seq::ir;
const Value *
convertPipelineToForLoopsHelper(const std::vector<const PipelineFlow::Stage *> &stages,
                                unsigned idx = 0, const Value *last = nullptr) {
  if (idx >= stages.size())
    return last;
  auto *stage = stages[idx];
  auto *M = stage->getCallee()->getModule();
  Value *next = nullptr;
  if (last) {
    std::vector<Value *> args;
    for (auto *arg : *stage) {
      args.push_back(const_cast<Value *>(arg ? arg : last));
    }
    next = M->Nr<CallInstr>(const_cast<Value *>(stage->getCallee()), args);
  } else {
    next = const_cast<Value *>(stage->getCallee());
  }
  if (stage->isGenerator()) {
    auto *var = M->Nr<Var>(stage->getOutputElementType());
    Value *body = const_cast<Value *>(
        convertPipelineToForLoopsHelper(stages, idx + 1, M->Nr<VarValue>(var)));
    auto *s = M->Nr<SeriesFlow>();
    s->push_back(body);

    return M->Nr<ForFlow>(next, s, var);
  } else {
    return convertPipelineToForLoopsHelper(stages, idx + 1, next);
  }
}

const Value *convertPipelineToForLoops(const PipelineFlow *p) {
  std::vector<const PipelineFlow::Stage *> stages;
  for (const auto &stage : *p) {
    stages.push_back(&stage);
  }
  return convertPipelineToForLoopsHelper(stages);
}
} // namespace

namespace seq {
namespace ir {
namespace analyze {
namespace dataflow {

void CFBlock::reg(const Value *v) { graph->valueLocations[v->getId()] = this; }

const char SyntheticAssignInstr::NodeId = 0;

int SyntheticAssignInstr::doReplaceUsedValue(id_t id, Value *newValue) {
  if (arg && arg->getId() == id) {
    arg = newValue;
    return 1;
  }
  return 0;
}

int SyntheticAssignInstr::doReplaceUsedVariable(id_t id, Var *newVar) {
  if (lhs->getId() == id) {
    lhs = newVar;
    return 1;
  }
  return 0;
}

const char SyntheticPhiInstr::NodeId = 0;

std::vector<Value *> SyntheticPhiInstr::doGetUsedValues() const {
  std::vector<Value *> ret;
  for (auto &p : *this) {
    ret.push_back(const_cast<Value *>(p.getResult()));
  }
  return ret;
}

int SyntheticPhiInstr::doReplaceUsedValue(id_t id, Value *newValue) {
  auto res = 0;
  for (auto &p : *this) {
    if (p.getResult()->getId() == id) {
      p.setResult(newValue);
      ++res;
    }
  }
  return res;
}

CFGraph::CFGraph(const BodiedFunc *f) : func(f) { newBlock("entry", true); }

std::unique_ptr<CFGraph> buildCFGraph(const BodiedFunc *f) {
  auto ret = std::make_unique<CFGraph>(f);
  CFVisitor v(ret.get());
  v.process(f);
  return ret;
}

std::unique_ptr<Result> CFAnalysis::run(const Module *m) {
  auto res = std::make_unique<CFResult>();
  if (const auto *main = cast<BodiedFunc>(m->getMainFunc())) {
    res->graphs.insert(std::make_pair(main->getId(), buildCFGraph(main)));
  }

  for (const auto *var : *m) {
    if (const auto *f = cast<BodiedFunc>(var)) {
      res->graphs.insert(std::make_pair(f->getId(), buildCFGraph(f)));
    }
  }
  return res;
}

void CFVisitor::visit(const SeriesFlow *v) {
  for (auto *c : *v) {
    process(c);
  }
}

void CFVisitor::visit(const IfFlow *v) {
  process(v->getCond());
  auto *original = graph->getCurrentBlock();
  auto *end = graph->newBlock("endIf");

  auto *tBranch = graph->newBlock("trueBranch", true);
  process(v->getTrueBranch());
  graph->getCurrentBlock()->successors_insert(end);

  analyze::dataflow::CFBlock *fBranch = nullptr;
  if (v->getFalseBranch()) {
    fBranch = graph->newBlock("falseBranch", true);
    process(v->getFalseBranch());
    graph->getCurrentBlock()->successors_insert(end);
  }

  original->successors_insert(tBranch);
  if (fBranch)
    original->successors_insert(fBranch);
  else
    original->successors_insert(end);

  graph->setCurrentBlock(end);
}

void CFVisitor::visit(const WhileFlow *v) {
  auto *original = graph->getCurrentBlock();
  auto *end = graph->newBlock("endIf");

  auto *loopBegin = graph->newBlock("whileBegin", true);
  original->successors_insert(loopBegin);
  process(v->getCond());
  graph->getCurrentBlock()->successors_insert(end);

  loopStack.emplace_back(loopBegin, end);
  auto *body = graph->newBlock("whileBody", true);
  loopBegin->successors_insert(body);
  process(v->getBody());
  body->successors_insert(loopBegin);
  loopStack.pop_back();

  graph->setCurrentBlock(end);
}

void CFVisitor::visit(const ForFlow *v) {
  auto *original = graph->getCurrentBlock();
  auto *end = graph->newBlock("endFor");

  auto *loopBegin = graph->newBlock("forBegin", true);
  original->successors_insert(loopBegin);
  process(v->getIter());

  auto *loopCheck = graph->newBlock("forCheck");
  graph->getCurrentBlock()->successors_insert(loopCheck);
  loopCheck->successors_insert(end);

  auto *loopNext = graph->newBlock("forNext");
  loopCheck->successors_insert(loopNext);
  loopNext->push_back(graph->N<analyze::dataflow::SyntheticAssignInstr>(
      const_cast<Var *>(v->getVar()), const_cast<Value *>(v->getIter()),
      analyze::dataflow::SyntheticAssignInstr::NEXT_VALUE));

  loopStack.emplace_back(loopCheck, end);
  auto *loopBody = graph->newBlock("forBody", true);
  loopNext->successors_insert(loopBody);
  process(v->getBody());
  graph->getCurrentBlock()->successors_insert(loopCheck);
  loopStack.pop_back();

  graph->setCurrentBlock(end);
}

void CFVisitor::visit(const ImperativeForFlow *v) {
  auto *original = graph->getCurrentBlock();
  auto *end = graph->newBlock("endFor");

  auto *loopBegin = graph->newBlock("forBegin", true);
  original->successors_insert(loopBegin);
  process(v->getStart());
  loopBegin->push_back(graph->N<analyze::dataflow::SyntheticAssignInstr>(
      const_cast<Var *>(v->getVar()), const_cast<Value *>(v->getStart()),
      analyze::dataflow::SyntheticAssignInstr::KNOWN));

  auto *loopCheck = graph->newBlock("forCheck");
  graph->getCurrentBlock()->successors_insert(loopCheck);
  loopCheck->successors_insert(end);

  auto *loopNext = graph->newBlock("forUpdate");
  loopNext->push_back(graph->N<analyze::dataflow::SyntheticAssignInstr>(
      const_cast<Var *>(v->getVar()), v->getStep()));
  loopNext->successors_insert(loopCheck);

  loopStack.emplace_back(loopCheck, end);
  auto *loopBody = graph->newBlock("forBody", true);
  loopCheck->successors_insert(loopBody);
  process(v->getBody());
  graph->getCurrentBlock()->successors_insert(loopCheck);
  loopStack.pop_back();

  graph->setCurrentBlock(end);
}

void CFVisitor::visit(const TryCatchFlow *v) {
  auto *routeBlock = graph->newBlock("tcRoute");
  auto *end = graph->newBlock("tcEnd");
  analyze::dataflow::CFBlock *finally = nullptr;
  if (v->getFinally())
    finally = graph->newBlock("tcFinally");

  auto *dst = finally ? finally : end;

  tryCatchStack.push_back(routeBlock);
  process(v->getBody());
  tryCatchStack.pop_back();
  graph->getCurrentBlock()->successors_insert(dst);

  if (v->getFinally()) {
    graph->setCurrentBlock(finally);
    process(v->getFinally());
    graph->getCurrentBlock()->successors_insert(end);
  }

  for (auto &c : *v) {
    auto *cBlock = graph->newBlock("catch", true);
    if (c.getVar())
      cBlock->push_back(graph->N<analyze::dataflow::SyntheticAssignInstr>(
          const_cast<Var *>(c.getVar())));
    process(c.getHandler());
    routeBlock->successors_insert(cBlock);
    graph->getCurrentBlock()->successors_insert(dst);
  }

  if (!tryCatchStack.empty())
    routeBlock->successors_insert(tryCatchStack.back());

  graph->setCurrentBlock(end);
}

void CFVisitor::visit(const PipelineFlow *v) {
  if (auto *loops = convertPipelineToForLoops(v)) {
    process(loops);
  } else {
    // pipeline is empty
  }
}

void CFVisitor::visit(const dsl::CustomFlow *v) {
  v->getCFBuilder()->buildCFNodes(this);
}

void CFVisitor::visit(const AssignInstr *v) {
  process(v->getRhs());
  defaultInsert(v);
}

void CFVisitor::visit(const ExtractInstr *v) {
  process(v->getVal());
  defaultInsert(v);
}

void CFVisitor::visit(const InsertInstr *v) {
  process(v->getLhs());
  process(v->getRhs());
  defaultInsert(v);
}

void CFVisitor::visit(const CallInstr *v) {
  process(v->getCallee());
  for (auto *a : *v)
    process(a);
  defaultInsert(v);
}

void CFVisitor::visit(const TernaryInstr *v) {
  auto *end = graph->newBlock("ternaryDone");
  auto *tBranch = graph->newBlock("ternaryTrue");
  auto *fBranch = graph->newBlock("ternaryFalse");

  process(v->getCond());
  graph->getCurrentBlock()->successors_insert(tBranch);
  graph->getCurrentBlock()->successors_insert(fBranch);

  graph->setCurrentBlock(tBranch);
  process(v->getTrueValue());
  graph->getCurrentBlock()->successors_insert(end);

  graph->setCurrentBlock(fBranch);
  process(v->getFalseValue());
  graph->getCurrentBlock()->successors_insert(end);

  auto *phi = graph->N<analyze::dataflow::SyntheticPhiInstr>();
  phi->emplace_back(tBranch, const_cast<Value *>(v->getTrueValue()));
  phi->emplace_back(fBranch, const_cast<Value *>(v->getFalseValue()));

  end->push_back(phi);
  graph->remapValue(v, phi);
  graph->setCurrentBlock(end);
}

void CFVisitor::visit(const BreakInstr *v) {
  graph->getCurrentBlock()->successors_insert(loopStack.back().end);
  defaultInsert(v);
}

void CFVisitor::visit(const ContinueInstr *v) {
  graph->getCurrentBlock()->successors_insert(loopStack.back().nextIt);
  defaultInsert(v);
}

void CFVisitor::visit(const ReturnInstr *v) {
  if (v->getValue())
    process(v->getValue());
  defaultInsert(v);
}

void CFVisitor::visit(const YieldInstr *v) {
  if (v->getValue())
    process(v->getValue());
  defaultInsert(v);
}

void CFVisitor::visit(const ThrowInstr *v) {
  if (v->getValue())
    process(v->getValue());
  defaultInsert(v);
}

void CFVisitor::visit(const FlowInstr *v) {
  process(v->getFlow());
  process(v);
}

void CFVisitor::visit(const dsl::CustomInstr *v) {
  v->getCFBuilder()->buildCFNodes(this);
}

} // namespace dataflow
} // namespace analyze
} // namespace ir
} // namespace seq

#undef DEFAULT_VISIT
