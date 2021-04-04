#include "cfg.h"

#include <vector>

#include "sir/dsl/codegen.h"
#include "sir/dsl/nodes.h"
#include "sir/util/visitor.h"

#define DEFAULT_VISIT(x)                                                               \
  void visit(const x *v) override { defaultInsert(v); }

namespace {
using namespace seq::ir;

class CFVisitor : public util::ConstVisitor {
private:
  struct Loop {
    analyze::dataflow::CFBlock *nextIt;
    analyze::dataflow::CFBlock *end;

    Loop(analyze::dataflow::CFBlock *nextIt, analyze::dataflow::CFBlock *end) : nextIt(nextIt), end(end) {}
  };

  analyze::dataflow::CFGraph *graph;
  std::vector<analyze::dataflow::CFBlock *> tryCatchStack;
  std::unordered_set<int> seenIds;
  std::vector<Loop> loopStack;

public:
  explicit CFVisitor(analyze::dataflow::CFGraph *graph) : graph(graph) {}

  void visit(const BodiedFunc *f) override { process(f->getBody()); }

  DEFAULT_VISIT(VarValue)
  DEFAULT_VISIT(PointerValue)

  void visit(const SeriesFlow *v) override {
    for (auto *c : *v) {
      process(c);
    }
  }
  void visit(const IfFlow *v) override {
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
  void visit(const WhileFlow *v) override {
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
  void visit(const ForFlow *v) override {
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
  void visit(const TryCatchFlow *v) override {
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
        cBlock->push_back(
            graph->N<analyze::dataflow::SyntheticAssignInstr>(const_cast<Var *>(c.getVar())));
      process(c.getHandler());
      routeBlock->successors_insert(cBlock);
      graph->getCurrentBlock()->successors_insert(dst);
    }

    if (!tryCatchStack.empty())
      routeBlock->successors_insert(tryCatchStack.back());

    graph->setCurrentBlock(end);
  }
  void visit(const PipelineFlow *v) override {
    // TODO
    assert(false);
  }
  void visit(const dsl::CustomFlow *v) override {
    v->getCFBuilder()->buildCFNodes(graph);
  }

  DEFAULT_VISIT(TemplatedConst<int64_t>);
  DEFAULT_VISIT(TemplatedConst<double>);
  DEFAULT_VISIT(TemplatedConst<bool>);
  DEFAULT_VISIT(TemplatedConst<std::string>);
  DEFAULT_VISIT(dsl::CustomConst);

  void visit(const AssignInstr *v) override {
    process(v->getRhs());
    defaultInsert(v);
  }
  void visit(const ExtractInstr *v) override {
    process(v->getVal());
    defaultInsert(v);
  }
  void visit(const InsertInstr *v) override {
    process(v->getLhs());
    process(v->getRhs());
    defaultInsert(v);
  }
  void visit(const CallInstr *v) override {
    process(v->getCallee());
    for (auto *a : *v)
      process(a);
    defaultInsert(v);
  }
  DEFAULT_VISIT(StackAllocInstr);
  DEFAULT_VISIT(TypePropertyInstr);
  DEFAULT_VISIT(YieldInInstr);
  void visit(const TernaryInstr *v) override {
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
    graph->setCurrentBlock(end);
  }
  void visit(const BreakInstr *v) override {
    graph->getCurrentBlock()->successors_insert(loopStack.back().end);
    defaultInsert(v);
  }
  void visit(const ContinueInstr *v) override {
    graph->getCurrentBlock()->successors_insert(loopStack.back().nextIt);
    defaultInsert(v);
  }
  void visit(const ReturnInstr *v) override {
    if (v->getValue())
      process(v->getValue());
    defaultInsert(v);
  }
  void visit(const YieldInstr *v) override {
    if (v->getValue())
      process(v->getValue());
    defaultInsert(v);
  }
  void visit(const ThrowInstr *v) override {
    if (v->getValue())
      process(v->getValue());
    defaultInsert(v);
  }
  void visit(const FlowInstr *v) override {
    process(v->getFlow());
    process(v);
  }
  void visit(const dsl::CustomInstr *v) override {
    v->getCFBuilder()->buildCFNodes(graph);
  }

  template <typename NodeType> void process(const NodeType *v) {
    if (seenIds.find(v->getId()) != seenIds.end())
      return;
    seenIds.insert(v->getId());
    v->accept(*this);
  }

private:
  void defaultInsert(const Value *v) {
    if (tryCatchStack.empty()) {
      graph->getCurrentBlock()->push_back(v);
    } else {
      auto *original = graph->getCurrentBlock();
      auto *newBlock = graph->newBlock("", true);
      original->successors_insert(newBlock);
      newBlock->successors_insert(tryCatchStack.back());
    }
    seenIds.insert(v->getId());
  }
};

} // namespace

namespace seq {
namespace ir {
namespace analyze {
namespace dataflow {

void CFBlock::reg(const Value *v) {
  graph->valueLocations[v->getId()] = this;
}

const char SyntheticAssignInstr::NodeId = 0;

int SyntheticAssignInstr::doReplaceUsedValue(int id, Value *newValue) {
  if (arg && arg->getId() == id) {
    arg = newValue;
    return 1;
  }
  return 0;
}

int SyntheticAssignInstr::doReplaceUsedVariable(int id, Var *newVar) {
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

int SyntheticPhiInstr::doReplaceUsedValue(int id, Value *newValue) {
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

} // namespace dataflow
} // namespace analyze
} // namespace ir
} // namespace seq

#undef DEFAULT_VISIT
