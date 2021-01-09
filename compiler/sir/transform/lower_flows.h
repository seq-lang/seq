#pragma once

#include <unordered_map>

#include "sir/util/context.h"
#include "sir/util/visitor.h"

#include "sir/sir.h"

namespace seq {
namespace ir {
namespace transform {

class LowerFlowsVisitor : public util::SIRVisitor {
private:
  struct Loop {
    Value *continueDst;
    Value *breakDst;
  };

  struct Context : public util::SIRContext<UnorderedFlow *> {
    IRModule *module = nullptr;
    BodiedFunc *curFunc = nullptr;
    std::unordered_map<int, Value *> mappings;
    std::unordered_map<int, Loop> loopMappings;

    SeriesFlow *getInsertPoint();
  };

  std::shared_ptr<Context> ctx;
  bool flatten = false;
  ValuePtr result;

public:
  explicit LowerFlowsVisitor() : ctx(std::make_shared<Context>()) {}

  LowerFlowsVisitor(const LowerFlowsVisitor &other)
      : ctx(other.ctx), flatten(other.flatten) {}

  void visit(IRModule *module) override;

  void defaultVisit(Var *v) override;
  void defaultVisit(Func *f) override;
  void visit(BodiedFunc *f) override;

  void defaultVisit(Value *v) override;
  void visit(ValueProxy *v) override;

  void visit(SeriesFlow *i) override;
  void visit(IfFlow *i) override;
  void visit(WhileFlow *i) override;
  void visit(ForFlow *i) override;
  void visit(TryCatchFlow *i) override;

  void defaultVisit(Constant *c) override;

  void defaultVisit(Instr *i) override;
  void visit(AssignInstr *i) override;
  void visit(ExtractInstr *i) override;
  void visit(InsertInstr *i) override;
  void visit(CallInstr *i) override;
  void visit(StackAllocInstr *i) override;
  void visit(TernaryInstr *i) override;
  void visit(BreakInstr *i) override;
  void visit(ContinueInstr *i) override;
  void visit(ReturnInstr *i) override;
  void visit(YieldInstr *i) override;
  void visit(ThrowInstr *i) override;
  void visit(FlowInstr *i) override;

  template <typename T, typename... Args>
  decltype(auto) process(const std::unique_ptr<T> &x, Args &&... args) {
    return process(x.get(), std::forward<Args>(args)...);
  }

  static void process(IRModule *module);

  void process(Var *v);
  ValuePtr process(Value *v, bool skipFlatten = false);

  static bool needsUnroll(Value *v);
  bool setTerminator(ValuePtr t);

  ValuePtr makeProxy(Value *other, SrcInfo s = {});

  template <typename T>
  ValuePtr makeProxy(const std::unique_ptr<T> &other, SrcInfo s = {}) {
    return makeProxy(other.get(), std::move(s));
  }
};

} // namespace transform
} // namespace ir
} // namespace seq