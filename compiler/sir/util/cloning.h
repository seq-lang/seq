#pragma once

#include "sir/sir.h"
#include "visitor.h"

namespace seq {
namespace ir {
namespace util {

class CloneVisitor : public ConstIRVisitor {
private:
  /// the clone context
  std::unordered_map<int, IRNode *> ctx;
  /// the result
  IRNode *result = nullptr;
  /// the module
  IRModule *module;

public:
  /// Constructs a clone visitor.
  /// @param module the module
  explicit CloneVisitor(IRModule *module) : module(module) {}

  virtual ~CloneVisitor() noexcept = default;

  void visit(const Var *v) override;

  void visit(const BodiedFunc *v) override;
  void visit(const ExternalFunc *v) override;
  void visit(const InternalFunc *v) override;
  void visit(const LLVMFunc *v) override;

  void visit(const VarValue *v) override;
  void visit(const PointerValue *v) override;

  void visit(const SeriesFlow *v) override;
  void visit(const IfFlow *v) override;
  void visit(const WhileFlow *v) override;
  void visit(const ForFlow *v) override;
  void visit(const TryCatchFlow *v) override;
  void visit(const PipelineFlow *v) override;
  void visit(const dsl::CustomFlow *v) override;

  void visit(const IntConstant *v) override;
  void visit(const FloatConstant *v) override;
  void visit(const BoolConstant *v) override;
  void visit(const StringConstant *v) override;
  void visit(const dsl::CustomConstant *v) override;

  void visit(const AssignInstr *v) override;
  void visit(const ExtractInstr *v) override;
  void visit(const InsertInstr *v) override;
  void visit(const CallInstr *v) override;
  void visit(const StackAllocInstr *v) override;
  void visit(const TypePropertyInstr *v) override;
  void visit(const YieldInInstr *v) override;
  void visit(const TernaryInstr *v) override;
  void visit(const BreakInstr *v) override;
  void visit(const ContinueInstr *v) override;
  void visit(const ReturnInstr *v) override;
  void visit(const YieldInstr *v) override;
  void visit(const ThrowInstr *v) override;
  void visit(const FlowInstr *v) override;
  void visit(const dsl::CustomInstr *v) override;

  template <typename NodeType> NodeType *clone(const NodeType *other) {
    if (!other)
      return nullptr;

    auto id = other->getId();
    if (ctx.find(id) == ctx.end()) {
      other->accept(*this);
      ctx[id] = result;

      for (auto it = other->attributes_begin(); it != other->attributes_end(); ++it) {
        const auto *attr = other->getAttribute(*it);
        if (attr->needsClone()) {
          ctx[id]->setAttribute(attr->clone(), *it);
        }
      }
    }
    return cast<NodeType>(ctx[id]);
  }

  PipelineFlow::Stage clone(const PipelineFlow::Stage &other) {
    std::vector<Value *> args;
    for (const auto *a : other)
      args.push_back(clone(a));
    return {clone(other.getFunc()), std::move(args), other.isGenerator(),
            other.isParallel()};
  }

private:
  template <typename NodeType, typename... Args>
  NodeType *Nt(const NodeType *source, Args... args) {
    return module->N<NodeType>(source, std::forward<Args>(args)..., source->getName());
  }
};

} // namespace util
} // namespace ir
} // namespace seq
