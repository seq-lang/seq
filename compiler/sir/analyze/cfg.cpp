#include "cfg.h"

#include "sir/util/visitor.h"

#define DEFAULT_VISIT(x) void visit(const x *v) override { defaultInsert(v); }

namespace {
using namespace seq::ir;

class CFVisitor : public util::ConstIRVisitor {
private:
  analyze::CFGraph *graph;
  std::vector<analyze::CFBlock *> tryCatchStack;

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

    auto *tBranch = graph->newBlock("trueBranch");
    process(v->getTrueBranch());
    graph->getCurrentBlock()->successors_insert(end);

    analyze::CFBlock *fBranch = nullptr;
    if (v->getFalseBranch()) {
      fBranch = graph->newBlock("falseBranch");
      process(v->getFalseBranch());
      graph->getCurrentBlock()->successors_insert(end);
    }

    original->successors_insert(tBranch);
    if (fBranch)
      original->successors_insert(fBranch);

    graph->setCurrentBlock(end);
  }
  void visit(const WhileFlow *v) override {
    process(v->getCond());
    auto *original = graph->getCurrentBlock();
    auto *end = graph->newBlock("endIf");

    auto *loopBegin = graph->newBlock("whileBegin");
    original->successors_insert(loopBegin);
    process(v->getCond());
    graph->getCurrentBlock()->successors_insert(end);

    auto *body = graph->newBlock("whileBody");
    loopBegin->successors_insert(body);
    process(v->getBody());
    body->successors_insert(loopBegin);

    graph->setCurrentBlock(end);
  }
  void visit(const ForFlow *v) override {}
  void visit(const TryCatchFlow *v) override {}
  void visit(const PipelineFlow *v) override {}
  void visit(const dsl::CustomFlow *v) override {}

  void visit(const TemplatedConstant<int64_t> *v) override {}
  void visit(const TemplatedConstant<double> *v) override {}
  void visit(const TemplatedConstant<bool> *v) override {}
  void visit(const TemplatedConstant<std::string> *v) override {}
  void visit(const dsl::CustomConstant *v) override {}

  void visit(const AssignInstr *v) override {}
  void visit(const ExtractInstr *v) override {}
  void visit(const InsertInstr *v) override {}
  void visit(const CallInstr *v) override {}
  void visit(const StackAllocInstr *v) override {}
  void visit(const TypePropertyInstr *v) override {}
  void visit(const YieldInInstr *v) override {}
  void visit(const TernaryInstr *v) override {}
  void visit(const BreakInstr *v) override {}
  void visit(const ContinueInstr *v) override {}
  void visit(const ReturnInstr *v) override {}
  void visit(const YieldInstr *v) override {}
  void visit(const ThrowInstr *v) override {}
  void visit(const FlowInstr *v) override {}
  void visit(const dsl::CustomInstr *v) override {}

private:
  template <typename NodeType>
  void process(const NodeType *v) {
    v->accept(*this);
  }

  void defaultInsert(const Value *v) {
    if (tryCatchStack.empty()) {
      graph->getCurrentBlock()->push_back(v);
    } else {
      auto *original = graph->getCurrentBlock();
      auto *newBlock = graph->newBlock();
      original->successors_insert(newBlock);
      newBlock->successors_insert(tryCatchStack.back());
    }
  }
};

}

namespace seq {
namespace ir {
namespace analyze {

CFGraph::CFGraph() {
  newBlock("entry");
}

} // namespace analyze
} // namespace ir
} // namespace seq

#undef DEFAULT_VISIT