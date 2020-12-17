#pragma once

#include <memory>
#include <string>

#include "runtime/lib.h"

#define DEFAULT_VISIT(x)                                                               \
  virtual void visit(seq::ir::x *) { throw std::runtime_error("cannot visit node"); }

namespace seq {
namespace ir {

namespace types {
class Type;
class RecordType;
class RefType;
class FuncType;
class OptionalType;
class ArrayType;
class PointerType;
class GeneratorType;
class IntNType;
} // namespace types

class IRModule;

class Func;
class Var;

class Value;
class ValueProxy;

class Flow;
class BlockFlow;
class SeriesFlow;
class WhileFlow;
class ForFlow;
class TryCatchFlow;

class Constant;

template <typename ValueType> class TemplatedConstant;

class Instr;
class AssignInstr;
class LoadInstr;
class CallInstr;
class StackAllocInstr;
class YieldInInstr;
class TernaryInstr;
class BreakInstr;
class ContinueInstr;
class ReturnInstr;
class YieldInstr;
class ThrowInstr;
class AssertInstr;
class FlowInstr;

namespace util {

/// Base for SIR visitors
class SIRVisitor {
public:
  DEFAULT_VISIT(IRModule);

  DEFAULT_VISIT(Func);
  DEFAULT_VISIT(Var);

  DEFAULT_VISIT(Value);
  DEFAULT_VISIT(ValueProxy);

  DEFAULT_VISIT(Flow);
  DEFAULT_VISIT(BlockFlow);
  DEFAULT_VISIT(SeriesFlow);
  DEFAULT_VISIT(WhileFlow);
  DEFAULT_VISIT(ForFlow);
  DEFAULT_VISIT(TryCatchFlow);

  DEFAULT_VISIT(Constant);
  DEFAULT_VISIT(TemplatedConstant<seq_int_t>);
  DEFAULT_VISIT(TemplatedConstant<float>);
  DEFAULT_VISIT(TemplatedConstant<bool>);
  DEFAULT_VISIT(TemplatedConstant<std::string>);

  DEFAULT_VISIT(Instr);
  DEFAULT_VISIT(AssignInstr);
  DEFAULT_VISIT(LoadInstr);
  DEFAULT_VISIT(CallInstr);
  DEFAULT_VISIT(StackAllocInstr);
  DEFAULT_VISIT(YieldInInstr);
  DEFAULT_VISIT(TernaryInstr);
  DEFAULT_VISIT(BreakInstr);
  DEFAULT_VISIT(ContinueInstr);
  DEFAULT_VISIT(ReturnInstr);
  DEFAULT_VISIT(YieldInstr);
  DEFAULT_VISIT(ThrowInstr);
  DEFAULT_VISIT(AssertInstr);
  DEFAULT_VISIT(FlowInstr);

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

} // namespace util
} // namespace ir
} // namespace seq

#undef DEFAULT_VISIT