#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#define DEFAULT_VISIT(x)                                                               \
  virtual void defaultVisit(seq::ir::x *) {                                            \
    throw std::runtime_error("cannot visit node");                                     \
  }                                                                                    \
  virtual void visit(seq::ir::x *v) { defaultVisit(v); }

#define CONST_DEFAULT_VISIT(x)                                                         \
  virtual void defaultVisit(const seq::ir::x *) {                                      \
    throw std::runtime_error("cannot visit const node");                               \
  }                                                                                    \
  virtual void visit(const seq::ir::x *v) { defaultVisit(v); }

#define VISIT(x) virtual void visit(seq::ir::x *v)
#define CONST_VISIT(x) virtual void visit(const seq::ir::x *v)

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
class PointerType;
class GeneratorType;
class IntNType;
} // namespace types

namespace dsl {

namespace types {
class CustomType;
}

class CustomConst;
class CustomFlow;
class CustomInstr;
} // namespace dsl

class Module;

class Var;

class Func;
class BodiedFunc;
class ExternalFunc;
class InternalFunc;
class LLVMFunc;

class Value;
class VarValue;
class PointerValue;

class Flow;
class SeriesFlow;
class IfFlow;
class WhileFlow;
class ForFlow;
class ImperativeForFlow;
class TryCatchFlow;
class PipelineFlow;

class Const;

template <typename ValueType> class TemplatedConst;

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
class Visitor {
public:
  virtual ~Visitor() noexcept = default;

  DEFAULT_VISIT(Module);

  DEFAULT_VISIT(Var);

  DEFAULT_VISIT(Func);
  VISIT(BodiedFunc);
  VISIT(ExternalFunc);
  VISIT(InternalFunc);
  VISIT(LLVMFunc);

  DEFAULT_VISIT(Value);
  VISIT(VarValue);
  VISIT(PointerValue);

  DEFAULT_VISIT(Flow);
  VISIT(SeriesFlow);
  VISIT(IfFlow);
  VISIT(WhileFlow);
  VISIT(ForFlow);
  VISIT(ImperativeForFlow);
  VISIT(TryCatchFlow);
  VISIT(PipelineFlow);
  VISIT(dsl::CustomFlow);

  DEFAULT_VISIT(Const);
  VISIT(TemplatedConst<int64_t>);
  VISIT(TemplatedConst<double>);
  VISIT(TemplatedConst<bool>);
  VISIT(TemplatedConst<std::string>);
  VISIT(dsl::CustomConst);

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
  VISIT(dsl::CustomInstr);

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
  VISIT(types::PointerType);
  VISIT(types::GeneratorType);
  VISIT(types::IntNType);
  VISIT(dsl::types::CustomType);
};

class ConstVisitor {
public:
  virtual ~ConstVisitor() noexcept = default;

  CONST_DEFAULT_VISIT(Module);

  CONST_DEFAULT_VISIT(Var);

  CONST_DEFAULT_VISIT(Func);
  CONST_VISIT(BodiedFunc);
  CONST_VISIT(ExternalFunc);
  CONST_VISIT(InternalFunc);
  CONST_VISIT(LLVMFunc);

  CONST_DEFAULT_VISIT(Value);
  CONST_VISIT(VarValue);
  CONST_VISIT(PointerValue);

  CONST_DEFAULT_VISIT(Flow);
  CONST_VISIT(SeriesFlow);
  CONST_VISIT(IfFlow);
  CONST_VISIT(WhileFlow);
  CONST_VISIT(ForFlow);
  CONST_VISIT(ImperativeForFlow);
  CONST_VISIT(TryCatchFlow);
  CONST_VISIT(PipelineFlow);
  CONST_VISIT(dsl::CustomFlow);

  CONST_DEFAULT_VISIT(Const);
  CONST_VISIT(TemplatedConst<int64_t>);
  CONST_VISIT(TemplatedConst<double>);
  CONST_VISIT(TemplatedConst<bool>);
  CONST_VISIT(TemplatedConst<std::string>);
  CONST_VISIT(dsl::CustomConst);

  CONST_DEFAULT_VISIT(Instr);
  CONST_VISIT(AssignInstr);
  CONST_VISIT(ExtractInstr);
  CONST_VISIT(InsertInstr);
  CONST_VISIT(CallInstr);
  CONST_VISIT(StackAllocInstr);
  CONST_VISIT(TypePropertyInstr);
  CONST_VISIT(YieldInInstr);
  CONST_VISIT(TernaryInstr);
  CONST_VISIT(BreakInstr);
  CONST_VISIT(ContinueInstr);
  CONST_VISIT(ReturnInstr);
  CONST_VISIT(YieldInstr);
  CONST_VISIT(ThrowInstr);
  CONST_VISIT(FlowInstr);
  CONST_VISIT(dsl::CustomInstr);

  CONST_DEFAULT_VISIT(types::Type);
  CONST_VISIT(types::PrimitiveType);
  CONST_VISIT(types::IntType);
  CONST_VISIT(types::FloatType);
  CONST_VISIT(types::BoolType);
  CONST_VISIT(types::ByteType);
  CONST_VISIT(types::VoidType);
  CONST_VISIT(types::RecordType);
  CONST_VISIT(types::RefType);
  CONST_VISIT(types::FuncType);
  CONST_VISIT(types::OptionalType);
  CONST_VISIT(types::PointerType);
  CONST_VISIT(types::GeneratorType);
  CONST_VISIT(types::IntNType);
  CONST_VISIT(dsl::types::CustomType);
};

} // namespace util
} // namespace ir
} // namespace seq

#undef DEFAULT_VISIT
#undef CONST_DEFAULT_VISIT
#undef VISIT
#undef CONST_VISIT
