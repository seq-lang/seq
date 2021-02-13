#include "visitor.h"

#include "sir/sir.h"

namespace seq {
namespace ir {
namespace util {

void IRVisitor::visit(BodiedFunc *x) { defaultVisit(x); }
void IRVisitor::visit(ExternalFunc *x) { defaultVisit(x); }
void IRVisitor::visit(InternalFunc *x) { defaultVisit(x); }
void IRVisitor::visit(LLVMFunc *x) { defaultVisit(x); }
void IRVisitor::visit(VarValue *x) { defaultVisit(x); }
void IRVisitor::visit(PointerValue *x) { defaultVisit(x); }
void IRVisitor::visit(SeriesFlow *x) { defaultVisit(x); }
void IRVisitor::visit(IfFlow *x) { defaultVisit(x); }
void IRVisitor::visit(WhileFlow *x) { defaultVisit(x); }
void IRVisitor::visit(ForFlow *x) { defaultVisit(x); }
void IRVisitor::visit(TryCatchFlow *x) { defaultVisit(x); }
void IRVisitor::visit(PipelineFlow *x) { defaultVisit(x); }
void IRVisitor::visit(dsl::CustomFlow *x) { defaultVisit(x); }
void IRVisitor::visit(TemplatedConstant<int64_t> *x) { defaultVisit(x); }
void IRVisitor::visit(TemplatedConstant<double> *x) { defaultVisit(x); }
void IRVisitor::visit(TemplatedConstant<bool> *x) { defaultVisit(x); }
void IRVisitor::visit(TemplatedConstant<std::string> *x) { defaultVisit(x); }
void IRVisitor::visit(dsl::CustomConstant *x) { defaultVisit(x); }
void IRVisitor::visit(AssignInstr *x) { defaultVisit(x); }
void IRVisitor::visit(ExtractInstr *x) { defaultVisit(x); }
void IRVisitor::visit(InsertInstr *x) { defaultVisit(x); }
void IRVisitor::visit(CallInstr *x) { defaultVisit(x); }
void IRVisitor::visit(StackAllocInstr *x) { defaultVisit(x); }
void IRVisitor::visit(YieldInInstr *x) { defaultVisit(x); }
void IRVisitor::visit(TernaryInstr *x) { defaultVisit(x); }
void IRVisitor::visit(BreakInstr *x) { defaultVisit(x); }
void IRVisitor::visit(ContinueInstr *x) { defaultVisit(x); }
void IRVisitor::visit(ReturnInstr *x) { defaultVisit(x); }
void IRVisitor::visit(TypePropertyInstr *x) { defaultVisit(x); }
void IRVisitor::visit(YieldInstr *x) { defaultVisit(x); }
void IRVisitor::visit(ThrowInstr *x) { defaultVisit(x); }
void IRVisitor::visit(FlowInstr *x) { defaultVisit(x); }
void IRVisitor::visit(dsl::CustomInstr *x) { defaultVisit(x); }
void IRVisitor::visit(types::PrimitiveType *x) { defaultVisit(x); }
void IRVisitor::visit(types::IntType *x) { defaultVisit(x); }
void IRVisitor::visit(types::FloatType *x) { defaultVisit(x); }
void IRVisitor::visit(types::BoolType *x) { defaultVisit(x); }
void IRVisitor::visit(types::ByteType *x) { defaultVisit(x); }
void IRVisitor::visit(types::VoidType *x) { defaultVisit(x); }
void IRVisitor::visit(types::RecordType *x) { defaultVisit(x); }
void IRVisitor::visit(types::RefType *x) { defaultVisit(x); }
void IRVisitor::visit(types::FuncType *x) { defaultVisit(x); }
void IRVisitor::visit(types::OptionalType *x) { defaultVisit(x); }
void IRVisitor::visit(types::ArrayType *x) { defaultVisit(x); }
void IRVisitor::visit(types::PointerType *x) { defaultVisit(x); }
void IRVisitor::visit(types::GeneratorType *x) { defaultVisit(x); }
void IRVisitor::visit(types::IntNType *x) { defaultVisit(x); }
void IRVisitor::visit(dsl::types::CustomType *x) { defaultVisit(x); }

void ConstIRVisitor::visit(const BodiedFunc *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const ExternalFunc *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const InternalFunc *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const LLVMFunc *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const VarValue *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const PointerValue *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const SeriesFlow *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const IfFlow *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const WhileFlow *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const ForFlow *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const TryCatchFlow *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const PipelineFlow *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const dsl::CustomFlow *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const TemplatedConstant<int64_t> *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const TemplatedConstant<double> *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const TemplatedConstant<bool> *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const TemplatedConstant<std::string> *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const dsl::CustomConstant *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const AssignInstr *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const ExtractInstr *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const InsertInstr *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const CallInstr *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const StackAllocInstr *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const YieldInInstr *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const TernaryInstr *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const BreakInstr *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const ContinueInstr *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const ReturnInstr *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const TypePropertyInstr *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const YieldInstr *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const ThrowInstr *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const FlowInstr *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const dsl::CustomInstr *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const types::PrimitiveType *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const types::IntType *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const types::FloatType *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const types::BoolType *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const types::ByteType *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const types::VoidType *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const types::RecordType *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const types::RefType *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const types::FuncType *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const types::OptionalType *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const types::ArrayType *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const types::PointerType *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const types::GeneratorType *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const types::IntNType *x) { defaultVisit(x); }
void ConstIRVisitor::visit(const dsl::types::CustomType *x) { defaultVisit(x); }

} // namespace util
} // namespace ir
} // namespace seq
