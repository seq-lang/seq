#pragma once

#include <unordered_set>

#include "sir/sir.h"

#include "visitor.h"

#define LAMBDA_VISIT(x)                                                                \
  virtual void handle(seq::ir::x *v) {}                                                \
  void visit(seq::ir::x *v) override {                                                 \
    handle(v);                                                                         \
    processChildren(v);                                                                \
  }

namespace seq {
namespace ir {
namespace util {

/// Pass that visits all values in a module.
class LambdaValueVisitor : public IRVisitor {
private:
  /// IDs of previously visited nodes
  std::unordered_set<int> seen;
  /// stack of IR nodes being visited
  std::vector<IRNode *> stack;

public:
  void visit(IRModule *m) override {
    stack.push_back(m);
    process(m->getMainFunc());
    for (auto *s : *m)
      process(s);
    stack.pop_back();
  }

  void defaultVisit(Var *v) override {}
  void defaultVisit(Func *f) override {}

  void visit(BodiedFunc *f) override {
    seen.insert(f->getBody()->getId());
    process(f->getBody());
  }

  LAMBDA_VISIT(VarValue);
  LAMBDA_VISIT(PointerValue);

  LAMBDA_VISIT(SeriesFlow);
  LAMBDA_VISIT(IfFlow);
  LAMBDA_VISIT(WhileFlow);
  LAMBDA_VISIT(ForFlow);
  LAMBDA_VISIT(TryCatchFlow);
  LAMBDA_VISIT(PipelineFlow);
  LAMBDA_VISIT(dsl::CustomFlow);

  LAMBDA_VISIT(TemplatedConstant<int64_t>);
  LAMBDA_VISIT(TemplatedConstant<double>);
  LAMBDA_VISIT(TemplatedConstant<bool>);
  LAMBDA_VISIT(TemplatedConstant<std::string>);
  LAMBDA_VISIT(dsl::CustomConstant);

  LAMBDA_VISIT(Instr);
  LAMBDA_VISIT(AssignInstr);
  LAMBDA_VISIT(ExtractInstr);
  LAMBDA_VISIT(InsertInstr);
  LAMBDA_VISIT(CallInstr);
  LAMBDA_VISIT(StackAllocInstr);
  LAMBDA_VISIT(TypePropertyInstr);
  LAMBDA_VISIT(YieldInInstr);
  LAMBDA_VISIT(TernaryInstr);
  LAMBDA_VISIT(BreakInstr);
  LAMBDA_VISIT(ContinueInstr);
  LAMBDA_VISIT(ReturnInstr);
  LAMBDA_VISIT(YieldInstr);
  LAMBDA_VISIT(ThrowInstr);
  LAMBDA_VISIT(FlowInstr);
  LAMBDA_VISIT(dsl::CustomInstr);

  template <typename Node> void process(Node *v) { v->accept(*this); }

  /// Return the parent of the current node.
  /// @param level the number of levels up from the current node
  template <typename Desired = IRNode> Desired *getParent(int level = 0) {
    return cast<Desired>(stack[stack.size() - level - 1]);
  }

  /// @return current depth in the tree
  int depth() const { return stack.size(); }

private:
  void processChildren(Value *v) {
    stack.push_back(v);
    for (auto *c : v->getUsedValues()) {
      if (seen.find(c->getId()) != seen.end())
        continue;
      seen.insert(c->getId());
      process(c);
    }
    stack.pop_back();
  }
};

} // namespace util
} // namespace ir
} // namespace seq

#undef LAMBDA_VISIT
