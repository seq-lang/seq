#include "visitor.h"

#include "sir/sir.h"

namespace seq {
namespace ir {
namespace util {

void SIRVisitor::visit(BodiedFunc *x) { defaultVisit(x); }
void SIRVisitor::visit(const BodiedFunc *x) { defaultVisit(x); }
void SIRVisitor::visit(ExternalFunc *x) { defaultVisit(x); }
void SIRVisitor::visit(const ExternalFunc *x) { defaultVisit(x); }
void SIRVisitor::visit(InternalFunc *x) { defaultVisit(x); }
void SIRVisitor::visit(const InternalFunc *x) { defaultVisit(x); }
void SIRVisitor::visit(LLVMFunc *x) { defaultVisit(x); }
void SIRVisitor::visit(const LLVMFunc *x) { defaultVisit(x); }

void SIRVisitor::visit(VarValue *x) { defaultVisit(x); }
void SIRVisitor::visit(const VarValue *x) { defaultVisit(x); }
void SIRVisitor::visit(PointerValue *x) { defaultVisit(x); }
void SIRVisitor::visit(const PointerValue *x) { defaultVisit(x); }
void SIRVisitor::visit(ValueProxy *x) { defaultVisit(x); }
void SIRVisitor::visit(const ValueProxy *x) { defaultVisit(x); }

void SIRVisitor::visit(SeriesFlow *x) { defaultVisit(x); }
void SIRVisitor::visit(const SeriesFlow *x) { defaultVisit(x); }
void SIRVisitor::visit(IfFlow *x) { defaultVisit(x); }
void SIRVisitor::visit(const IfFlow *x) { defaultVisit(x); }
void SIRVisitor::visit(WhileFlow *x) { defaultVisit(x); }
void SIRVisitor::visit(const WhileFlow *x) { defaultVisit(x); }
void SIRVisitor::visit(ForFlow *x) { defaultVisit(x); }
void SIRVisitor::visit(const ForFlow *x) { defaultVisit(x); }
void SIRVisitor::visit(TryCatchFlow *x) { defaultVisit(x); }
void SIRVisitor::visit(const TryCatchFlow *x) { defaultVisit(x); }
void SIRVisitor::visit(UnorderedFlow *x) { defaultVisit(x); }
void SIRVisitor::visit(const UnorderedFlow *x) { defaultVisit(x); }

void SIRVisitor::visit(TemplatedConstant<seq_int_t> *x) { defaultVisit(x); }
void SIRVisitor::visit(const TemplatedConstant<seq_int_t> *x) { defaultVisit(x); }
void SIRVisitor::visit(TemplatedConstant<double> *x) { defaultVisit(x); }
void SIRVisitor::visit(const TemplatedConstant<double> *x) { defaultVisit(x); }
void SIRVisitor::visit(TemplatedConstant<bool> *x) { defaultVisit(x); }
void SIRVisitor::visit(const TemplatedConstant<bool> *x) { defaultVisit(x); }
void SIRVisitor::visit(TemplatedConstant<std::string> *x) { defaultVisit(x); }
void SIRVisitor::visit(const TemplatedConstant<std::string> *x) { defaultVisit(x); }

void SIRVisitor::visit(AssignInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(const AssignInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(ExtractInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(const ExtractInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(InsertInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(const InsertInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(CallInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(const CallInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(StackAllocInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(const StackAllocInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(YieldInInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(const YieldInInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(TernaryInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(const TernaryInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(BreakInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(const BreakInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(ContinueInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(const ContinueInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(ReturnInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(const ReturnInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(TypePropertyInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(const TypePropertyInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(YieldInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(const YieldInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(ThrowInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(const ThrowInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(FlowInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(const FlowInstr *x) { defaultVisit(x); }

void SIRVisitor::visit(types::PrimitiveType *x) { defaultVisit(x); }
void SIRVisitor::visit(const types::PrimitiveType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::IntType *x) { defaultVisit(x); }
void SIRVisitor::visit(const types::IntType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::FloatType *x) { defaultVisit(x); }
void SIRVisitor::visit(const types::FloatType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::BoolType *x) { defaultVisit(x); }
void SIRVisitor::visit(const types::BoolType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::ByteType *x) { defaultVisit(x); }
void SIRVisitor::visit(const types::ByteType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::VoidType *x) { defaultVisit(x); }
void SIRVisitor::visit(const types::VoidType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::RecordType *x) { defaultVisit(x); }
void SIRVisitor::visit(const types::RecordType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::RefType *x) { defaultVisit(x); }
void SIRVisitor::visit(const types::RefType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::FuncType *x) { defaultVisit(x); }
void SIRVisitor::visit(const types::FuncType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::OptionalType *x) { defaultVisit(x); }
void SIRVisitor::visit(const types::OptionalType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::ArrayType *x) { defaultVisit(x); }
void SIRVisitor::visit(const types::ArrayType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::PointerType *x) { defaultVisit(x); }
void SIRVisitor::visit(const types::PointerType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::GeneratorType *x) { defaultVisit(x); }
void SIRVisitor::visit(const types::GeneratorType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::IntNType *x) { defaultVisit(x); }
void SIRVisitor::visit(const types::IntNType *x) { defaultVisit(x); }

} // namespace util
} // namespace ir
} // namespace seq
