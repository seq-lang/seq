#include "cfg.h"

#include <vector>

#include "sir/dsl/codegen.h"
#include "sir/dsl/nodes.h"
#include "sir/util/irtools.h"
#include "sir/util/visitor.h"

#define DEFAULT_VISIT(x)                                                               \
  void visit(const x *v) override { defaultInsert(v); }

namespace {
using namespace seq::ir;

const Value *
convertPipelineToForLoopsHelper(const std::vector<const PipelineFlow::Stage *> &stages,
                                unsigned idx = 0, const Value *last = nullptr) {
  if (idx >= stages.size())
    return last;
  auto *stage = stages[idx];
  auto *M = stage->getFunc()->getModule();
  Value *next = nullptr;
  if (last) {
    std::vector<Value *> args;
    for (auto *arg : *stage) {
      args.push_back(const_cast<Value *>(arg ? arg : last));
    }
    next = M->Nr<CallInstr>(const_cast<Value *>(stage->getFunc()), args);
  } else {
    next = const_cast<Value *>(stage->getFunc());
  }
  if (stage->isGenerator()) {
    auto *var = M->Nr<Var>(stage->getOutputElementType());
    Value *body = const_cast<Value *>(
        convertPipelineToForLoopsHelper(stages, idx + 1, M->Nr<VarValue>(var)));
    return M->Nr<ForFlow>(next, util::series(body), var);
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

class CFVisitor : public util::ConstIRVisitor {
private:
  struct Loop {
    analyze::CFBlock *nextIt;
    analyze::CFBlock *end;

    Loop(analyze::CFBlock *nextIt, analyze::CFBlock *end) : nextIt(nextIt), end(end) {}
  };

  analyze::CFGraph *graph;
  std::vector<analyze::CFBlock *> tryCatchStack;
  std::unordered_set<int> seenIds;
  std::vector<Loop> loopStack;

public:
  explicit CFVisitor(analyze::CFGraph *graph) : graph(graph) {}

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

    analyze::CFBlock *fBranch = nullptr;
    if (v->getFalseBranch()) {
      fBranch = graph->newBlock("falseBranch", true);
      process(v->getFalseBranch());
      graph->getCurrentBlock()->successors_insert(end);
    }

    original->successors_insert(tBranch);
    if (fBranch)
      original->successors_insert(fBranch);

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
    loopNext->push_back(graph->N<analyze::SyntheticAssignInstr>(
        const_cast<Var *>(v->getVar()), const_cast<Value *>(v->getIter()),
        analyze::SyntheticAssignInstr::NEXT_VALUE));

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
    analyze::CFBlock *finally = nullptr;
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
            graph->N<analyze::SyntheticAssignInstr>(const_cast<Var *>(c.getVar())));
      process(c.getHandler());
      routeBlock->successors_insert(cBlock);
      graph->getCurrentBlock()->successors_insert(dst);
    }

    if (!tryCatchStack.empty())
      routeBlock->successors_insert(tryCatchStack.back());

    graph->setCurrentBlock(end);
  }
  void visit(const PipelineFlow *v) override {
    if (auto *loops = convertPipelineToForLoops(v)) {
      process(loops);
    } else {
      // pipeline is empty
    }
  }
  void visit(const dsl::CustomFlow *v) override {
    v->getCFBuilder()->buildCFNodes(graph);
  }

  DEFAULT_VISIT(TemplatedConstant<int64_t>);
  DEFAULT_VISIT(TemplatedConstant<double>);
  DEFAULT_VISIT(TemplatedConstant<bool>);
  DEFAULT_VISIT(TemplatedConstant<std::string>);
  DEFAULT_VISIT(dsl::CustomConstant);

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
    process(v->getFunc());
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

    auto *phi = graph->N<analyze::SyntheticPhiInstr>();
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

std::ostream &SyntheticAssignInstr::doFormat(std::ostream &os) const {
  if (kind == UNKNOWN) {
    fmt::print(os, FMT_STRING("store({}, ?)"), *lhs);
  } else if (kind == KNOWN) {
    fmt::print(os, FMT_STRING("store({}, {})"), *lhs, *arg);
  } else {
    fmt::print(os, FMT_STRING("store({}, next({}))"), *lhs, *arg);
  }
  return os;
}

Value *SyntheticAssignInstr::doClone() const { assert(false); }

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

std::ostream &SyntheticPhiInstr::doFormat(std::ostream &os) const {
  std::vector<std::string> dst;
  for (auto &p : *this) {
    dst.push_back(
        fmt::format(FMT_STRING("dst({}, {})"), p.getPred()->getId(), *p.getResult()));
  }
  fmt::print(os, FMT_STRING("phi({})"), fmt::join(dst.begin(), dst.end(), ", "));
  return os;
}

Value *SyntheticPhiInstr::doClone() const { assert(false); }

CFGraph::CFGraph(const BodiedFunc *f) : func(f) { newBlock("entry", true); }

std::unique_ptr<CFGraph> buildCFGraph(const BodiedFunc *f) {
  auto ret = std::make_unique<CFGraph>(f);
  CFVisitor v(ret.get());
  v.process(f);
  return ret;
}

std::unique_ptr<Result> CFAnalysis::run(const IRModule *m) {
  auto res = std::make_unique<CFResult>();
  for (const auto *var : *m) {
    if (const auto *f = cast<BodiedFunc>(var))
      res->graphs.insert(std::make_pair(f->getId(), buildCFGraph(f)));
  }
  return res;
}
} // namespace analyze
} // namespace ir
} // namespace seq

#undef DEFAULT_VISIT
