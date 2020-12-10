#pragma once

#include <memory>

#include "sir/sir.h"

#include "util/fmt/format.h"

#define DEFAULT_VISIT(x)                                                               \
  virtual void visit(x *n) { throw std::runtime_error("cannot visit node"); }

namespace seq {
namespace ir {

namespace util {
using namespace seq::ir;

struct LLVMOperand;

/// Base for SIR visitors
class SIRVisitor {
public:
  DEFAULT_VISIT(SIRModule);

  DEFAULT_VISIT(Func);
  DEFAULT_VISIT(Var);

  DEFAULT_VISIT(Flow);
  DEFAULT_VISIT(BlockFlow);
  DEFAULT_VISIT(SeriesFlow);
  DEFAULT_VISIT(WhileFlow);
  DEFAULT_VISIT(ForFlow);
  DEFAULT_VISIT(TryCatchFlow);

  DEFAULT_VISIT(Instr);
  DEFAULT_VISIT(AssignInstr);
  DEFAULT_VISIT(RvalueInstr);
  DEFAULT_VISIT(BreakInstr);
  DEFAULT_VISIT(ContinueInstr);
  DEFAULT_VISIT(ReturnInstr);
  DEFAULT_VISIT(YieldInstr);

  DEFAULT_VISIT(Rvalue);
  DEFAULT_VISIT(MemberRvalue);
  DEFAULT_VISIT(CallRvalue);
  DEFAULT_VISIT(OperandRvalue);
  DEFAULT_VISIT(StackAllocRvalue);

  DEFAULT_VISIT(Lvalue);
  DEFAULT_VISIT(VarLvalue);
  DEFAULT_VISIT(VarMemberLvalue);

  DEFAULT_VISIT(Operand);
  DEFAULT_VISIT(VarOperand);
  DEFAULT_VISIT(VarPointerOperand);
  DEFAULT_VISIT(LiteralOperand);
  DEFAULT_VISIT(LLVMOperand);

  DEFAULT_VISIT(types::Type);
  DEFAULT_VISIT(types::RecordType);
  DEFAULT_VISIT(types::RefType);
  DEFAULT_VISIT(types::FuncType);
  DEFAULT_VISIT(types::OptionalType);
  DEFAULT_VISIT(types::ArrayType);
  DEFAULT_VISIT(types::PointerType);
  DEFAULT_VISIT(types::GeneratorType);
  DEFAULT_VISIT(types::IntNType);
};

/// CRTP Base for SIR visitors that return from transform.
template <typename Derived, typename Context, typename ModuleResult,
          typename FlowResult, typename VarResult, typename InstrResult,
          typename RvalResult, typename LvalResult, typename OpResult,
          typename TypeResult>
class CallbackIRVisitor : public SIRVisitor {
protected:
  std::shared_ptr<Context> ctx;
  ModuleResult moduleResult{};
  FlowResult flowResult{};
  VarResult varResult{};
  InstrResult instrResult{};
  RvalResult rvalResult{};
  LvalResult lvalResult{};
  OpResult opResult{};
  TypeResult typeResult{};

public:
  explicit CallbackIRVisitor(std::shared_ptr<Context> ctx) : ctx(std::move(ctx)) {}

  template <typename... Args>
  ModuleResult transform(const std::unique_ptr<ir::SIRModule> &module, Args... args) {
    Derived v(ctx, args...);
    module->accept(v);
    return v.moduleResult;
  }
  template <typename... Args>
  FlowResult transform(const std::unique_ptr<ir::Flow> &f, Args... args) {
    Derived v(ctx, args...);
    f->accept(v);
    return v.flowResult;
  }
  template <typename... Args>
  VarResult transform(const std::unique_ptr<ir::Var> &var, Args... args) {
    Derived v(ctx, args...);
    var->accept(v);
    return v.varResult;
  }
  template <typename... Args>
  InstrResult transform(const std::unique_ptr<ir::Instr> &instr, Args... args) {
    Derived v(ctx, args...);
    instr->accept(v);
    return v.instrResult;
  }
  template <typename... Args>
  RvalResult transform(const std::unique_ptr<ir::Rvalue> &rval, Args... args) {
    Derived v(ctx, args...);
    rval->accept(v);
    return v.rvalResult;
  }
  template <typename... Args>
  LvalResult transform(const std::unique_ptr<Lvalue> &lval, Args... args) {
    Derived v(ctx, args...);
    lval->accept(v);
    return v.lvalResult;
  }
  template <typename... Args>
  OpResult transform(const std::unique_ptr<Operand> &op, Args... args) {
    Derived v(ctx, args...);
    op->accept(v);
    return v.opResult;
  }
  template <typename... Args>
  TypeResult transform(const std::unique_ptr<types::Type> &typ, Args... args) {
    Derived v(ctx, args...);
    typ->accept(v);
    return v.typeResult;
  }
  template <typename... Args>
  ModuleResult transform(ir::SIRModule *module, Args... args) {
    Derived v(ctx, args...);
    module->accept(v);
    return v.moduleResult;
  }
  template <typename... Args> FlowResult transform(ir::Flow *f, Args... args) {
    Derived v(ctx, args...);
    f->accept(v);
    return v.flowResult;
  }
  template <typename... Args> VarResult transform(ir::Var *var, Args... args) {
    Derived v(ctx, args...);
    var->accept(v);
    return v.varResult;
  }
  template <typename... Args> InstrResult transform(ir::Instr *instr, Args... args) {
    Derived v(ctx, args...);
    instr->accept(v);
    return v.instrResult;
  }
  template <typename... Args> RvalResult transform(ir::Rvalue *rval, Args... args) {
    Derived v(ctx, args...);
    rval->accept(v);
    return v.rvalResult;
  }
  template <typename... Args> LvalResult transform(Lvalue *lval, Args... args) {
    Derived v(ctx, args...);
    lval->accept(v);
    return v.lvalResult;
  }
  template <typename... Args> OpResult transform(Operand *op, Args... args) {
    Derived v(ctx, args...);
    op->accept(v);
    return v.opResult;
  }
  template <typename... Args> TypeResult transform(types::Type *typ, Args... args) {
    Derived v(ctx, args...);
    typ->accept(v);
    return v.typeResult;
  }
};

} // namespace util
} // namespace ir
} // namespace seq

#undef DEFAULT_VISIT