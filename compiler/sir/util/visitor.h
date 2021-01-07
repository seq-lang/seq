#pragma once

#include <memory>
#include <string>

#include "runtime/lib.h"

#define DEFAULT_VISIT(x)                                                               \
  virtual void defaultVisit(seq::ir::x *) {                                            \
    throw std::runtime_error("cannot visit node");                                     \
  }                                                                                    \
  virtual void visit(seq::ir::x *v) { defaultVisit(v); }                               \
  virtual void defaultVisit(const seq::ir::x *) {                                      \
    throw std::runtime_error("cannot visit const node");                               \
  }                                                                                    \
  virtual void visit(const seq::ir::x *v) { defaultVisit(v); }

#define VISIT(x)                                                                       \
  virtual void visit(seq::ir::x *v);                                                   \
  virtual void visit(const seq::ir::x *v)
namespace seq {
namespace ir {

namespace types {
class Type;
class PrimitiveType;
class IntType;
class FloatType;
class BoolType;
class ByteType;
class VoidType;
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

class Var;

class Func;
class BodiedFunc;
class ExternalFunc;
class InternalFunc;
class LLVMFunc;

class Value;
class VarValue;
class PointerValue;
class ValueProxy;

class Flow;
class SeriesFlow;
class IfFlow;
class WhileFlow;
class ForFlow;
class TryCatchFlow;
class UnorderedFlow;

class Constant;

template <typename ValueType> class TemplatedConstant;

class Instr;
class AssignInstr;
class ExtractInstr;
class InsertInstr;
class CallInstr;
class StackAllocInstr;
class TypePropertyInstr;
class YieldInInstr;
class TernaryInstr;
class BreakInstr;
class ContinueInstr;
class ReturnInstr;
class YieldInstr;
class ThrowInstr;
class FlowInstr;

namespace util {

/// Base for SIR visitors
class SIRVisitor {
public:
  DEFAULT_VISIT(IRModule);

  DEFAULT_VISIT(Var);

  DEFAULT_VISIT(Func);
  VISIT(BodiedFunc);
  VISIT(ExternalFunc);
  VISIT(InternalFunc);
  VISIT(LLVMFunc);

  DEFAULT_VISIT(Value);
  VISIT(VarValue);
  VISIT(PointerValue);
  VISIT(ValueProxy);

  DEFAULT_VISIT(Flow);
  VISIT(SeriesFlow);
  VISIT(IfFlow);
  VISIT(WhileFlow);
  VISIT(ForFlow);
  VISIT(TryCatchFlow);
  VISIT(UnorderedFlow);

  DEFAULT_VISIT(Constant);
  VISIT(TemplatedConstant<seq_int_t>);
  VISIT(TemplatedConstant<double>);
  VISIT(TemplatedConstant<bool>);
  VISIT(TemplatedConstant<std::string>);

  DEFAULT_VISIT(Instr);
  VISIT(AssignInstr);
  VISIT(ExtractInstr);
  VISIT(InsertInstr);
  VISIT(CallInstr);
  VISIT(StackAllocInstr);
  VISIT(TypePropertyInstr);
  VISIT(YieldInInstr);
  VISIT(TernaryInstr);
  VISIT(BreakInstr);
  VISIT(ContinueInstr);
  VISIT(ReturnInstr);
  VISIT(YieldInstr);
  VISIT(ThrowInstr);
  VISIT(FlowInstr);

  DEFAULT_VISIT(types::Type);
  VISIT(types::PrimitiveType);
  VISIT(types::IntType);
  VISIT(types::FloatType);
  VISIT(types::BoolType);
  VISIT(types::ByteType);
  VISIT(types::VoidType);
  VISIT(types::RecordType);
  VISIT(types::RefType);
  VISIT(types::FuncType);
  VISIT(types::OptionalType);
  VISIT(types::ArrayType);
  VISIT(types::PointerType);
  VISIT(types::GeneratorType);
  VISIT(types::IntNType);
};

} // namespace util
} // namespace ir
} // namespace seq

#undef DEFAULT_VISIT
