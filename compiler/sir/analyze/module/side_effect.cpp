#include "side_effect.h"

#include "sir/util/irtools.h"
#include "sir/util/operator.h"

namespace seq {
namespace ir {
namespace analyze {
namespace module {
namespace {
struct VarUseAnalyzer : public util::Operator {
  std::unordered_map<id_t, long> varCounts;
  std::unordered_map<id_t, long> varAssignCounts;

  void preHook(Node *v) override {
    for (auto *var : v->getUsedVariables()) {
      ++varCounts[var->getId()];
    }
  }

  void handle(AssignInstr *v) override { ++varAssignCounts[v->getLhs()->getId()]; }
};

struct SideEfectAnalyzer : public util::ConstVisitor {
  static const std::string PURE_ATTR;
  std::unordered_map<id_t, bool> result;
  bool last;

  SideEfectAnalyzer() : util::ConstVisitor(), result(), last(true) {}

  template <typename T> bool has(const T *v) {
    return result.find(v->getId()) != result.end();
  }

  template <typename T> void set(const T *v, bool hasSideEffect) {
    result[v->getId()] = last = hasSideEffect;
  }

  template <typename T> bool process(const T *v) {
    if (!v)
      return false;
    if (has(v))
      return result[v->getId()];
    v->accept(*this);
    return last;
  }

  void visit(const Module *v) override {
    process(v->getMainFunc());
    for (auto *x : *v) {
      process(x);
    }
  }

  void visit(const Var *v) override { set(v, false); }

  void visit(const BodiedFunc *v) override {
    const bool pure = util::hasAttribute(v, PURE_ATTR);
    set(v, !pure); // avoid infinite recursion
    bool s = process(v->getBody());
    if (pure)
      s = false;
    set(v, s);
  }

  void visit(const ExternalFunc *v) override {
    set(v, !util::hasAttribute(v, PURE_ATTR));
  }

  void visit(const InternalFunc *v) override { set(v, false); }

  void visit(const LLVMFunc *v) override { set(v, !util::hasAttribute(v, PURE_ATTR)); }

  void visit(const VarValue *v) override { set(v, false); }

  void visit(const PointerValue *v) override { set(v, false); }

  void visit(const SeriesFlow *v) override {
    bool s = false;
    for (auto *x : *v) {
      s |= process(x);
    }
    set(v, s);
  }

  void visit(const IfFlow *v) override {
    set(v, process(v->getCond()) | process(v->getTrueBranch()) |
               process(v->getFalseBranch()));
  }

  void visit(const WhileFlow *v) override {
    set(v, process(v->getCond()) | process(v->getBody()));
  }

  void visit(const ForFlow *v) override {
    bool s = process(v->getIter()) | process(v->getBody());
    if (auto *sched = v->getSchedule()) {
      for (auto *x : sched->getUsedValues()) {
        s |= process(x);
      }
    }
    set(v, s);
  }

  void visit(const ImperativeForFlow *v) override {
    bool s = process(v->getStart()) | process(v->getEnd()) | process(v->getBody());
    if (auto *sched = v->getSchedule()) {
      for (auto *x : sched->getUsedValues()) {
        s |= process(x);
      }
    }
    set(v, s);
  }

  void visit(const TryCatchFlow *v) override {
    bool s = process(v->getBody()) | process(v->getFinally());
    for (auto &x : *v) {
      s |= process(x.getHandler());
    }
    set(v, s);
  }

  void visit(const PipelineFlow *v) override {
    bool s = false;
    for (auto &stage : *v) {
      // make sure we're treating this as a call
      if (auto *f = util::getFunc(stage.getCallee())) {
        s |= process(f);
      } else {
        // unknown function
        process(stage.getCallee());
        s = true;
      }

      for (auto *arg : stage) {
        s |= process(arg);
      }
    }
    set(v, s);
  }

  void visit(const dsl::CustomFlow *v) override { set(v, v->hasSideEffect()); }

  void visit(const IntConst *v) override { set(v, false); }

  void visit(const FloatConst *v) override { set(v, false); }

  void visit(const BoolConst *v) override { set(v, false); }

  void visit(const StringConst *v) override { set(v, false); }

  void visit(const dsl::CustomConst *v) override { set(v, false); }

  void visit(const AssignInstr *v) override {
    process(v->getRhs());
    set(v, true);
  }

  void visit(const ExtractInstr *v) override { set(v, process(v->getVal())); }

  void visit(const InsertInstr *v) override {
    process(v->getLhs());
    process(v->getRhs());
    set(v, true);
  }

  void visit(const CallInstr *v) override {
    bool s = false;
    for (auto *x : *v) {
      s |= process(x);
    }
    if (auto *f = util::getFunc(v->getCallee())) {
      s |= process(f);
    } else {
      // unknown function
      process(v->getCallee());
      s = true;
    }
    set(v, s);
  }

  void visit(const StackAllocInstr *v) override { set(v, false); }

  void visit(const TypePropertyInstr *v) override { set(v, false); }

  void visit(const YieldInInstr *v) override { set(v, true); }

  void visit(const TernaryInstr *v) override {
    set(v, process(v->getCond()) | process(v->getTrueValue()) |
               process(v->getFalseValue()));
  }

  void visit(const BreakInstr *v) override { set(v, true); }

  void visit(const ContinueInstr *v) override { set(v, true); }

  void visit(const ReturnInstr *v) override {
    process(v->getValue());
    set(v, true);
  }

  void visit(const YieldInstr *v) override {
    process(v->getValue());
    set(v, true);
  }

  void visit(const ThrowInstr *v) override {
    process(v->getValue());
    set(v, true);
  }

  void visit(const FlowInstr *v) override {
    set(v, process(v->getFlow()) | process(v->getValue()));
  }

  void visit(const dsl::CustomInstr *v) override { set(v, v->hasSideEffect()); }
};

const std::string SideEfectAnalyzer::PURE_ATTR = "std.internal.attributes.pure";
} // namespace

const std::string SideEffectAnalysis::KEY = "core-analyses-side-effect";

bool SideEffectResult::hasSideEffect(Value *v) const {
  auto it = result.find(v->getId());
  return it == result.end() || it->second;
}

long SideEffectResult::getUsageCount(Var *v) const {
  auto it = varCounts.find(v->getId());
  return (it == varCounts.end()) ? 0 : it->second;
}

bool SideEffectResult::isOnlyAssigned(Var *v) const {
  auto it1 = varCounts.find(v->getId());
  auto it2 = varAssignCounts.find(v->getId());
  auto count1 = (it1 != varCounts.end()) ? it1->second : 0;
  auto count2 = (it2 != varAssignCounts.end()) ? it2->second : 0;
  return count1 == count2;
}

std::unique_ptr<Result> SideEffectAnalysis::run(const Module *m) {
  VarUseAnalyzer vua;
  const_cast<Module *>(m)->accept(vua);
  SideEfectAnalyzer sea;
  m->accept(sea);
  return std::make_unique<SideEffectResult>(sea.result, vua.varCounts,
                                            vua.varAssignCounts);
}

} // namespace module
} // namespace analyze
} // namespace ir
} // namespace seq
