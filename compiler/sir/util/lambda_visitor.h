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

class LambdaValueVisitor : public IRVisitor {
private:
  std::unordered_set<int> seen;

public:
  void visit(IRModule *m) override {
    process(m->getMainFunc());
    for (auto *s : *m)
      process(s);
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

  LAMBDA_VISIT(TemplatedConstant<seq_int_t>);
  LAMBDA_VISIT(TemplatedConstant<double>);
  LAMBDA_VISIT(TemplatedConstant<bool>);
  LAMBDA_VISIT(TemplatedConstant<std::string>);

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

  template <typename Node> void process(Node *v) { v->accept(*this); }

  void processChildren(Value *v) {
    for (auto *c : v->getChildren()) {
      if (seen.find(c->getId()) != seen.end())
        continue;
      seen.insert(c->getId());
      process(c);
    }
  }
};

} // namespace util
} // namespace ir
} // namespace seq

#undef LAMBDA_VISIT