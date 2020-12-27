#include "visitor.h"

#include "sir/sir.h"

namespace seq {
namespace ir {
namespace util {

void SIRVisitor::visit(BodiedFunc *x) { defaultVisit(x); }
void SIRVisitor::visit(ExternalFunc *x) { defaultVisit(x); }
void SIRVisitor::visit(InternalFunc *x) { defaultVisit(x); }
void SIRVisitor::visit(LLVMFunc *x) { defaultVisit(x); }

void SIRVisitor::visit(VarValue *x) { defaultVisit(x); }
void SIRVisitor::visit(PointerValue *x) { defaultVisit(x); }
void SIRVisitor::visit(ValueProxy *x) { defaultVisit(x); }

void SIRVisitor::visit(SeriesFlow *x) { defaultVisit(x); }
void SIRVisitor::visit(IfFlow *x) { defaultVisit(x); }
void SIRVisitor::visit(WhileFlow *x) { defaultVisit(x); }
void SIRVisitor::visit(ForFlow *x) { defaultVisit(x); }
void SIRVisitor::visit(TryCatchFlow *x) { defaultVisit(x); }
void SIRVisitor::visit(UnorderedFlow *x) { defaultVisit(x); }

void SIRVisitor::visit(TemplatedConstant<seq_int_t> *x) { defaultVisit(x); }
void SIRVisitor::visit(TemplatedConstant<double> *x) { defaultVisit(x); }
void SIRVisitor::visit(TemplatedConstant<bool> *x) { defaultVisit(x); }
void SIRVisitor::visit(TemplatedConstant<std::string> *x) { defaultVisit(x); }

void SIRVisitor::visit(AssignInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(ExtractInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(InsertInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(CallInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(StackAllocInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(YieldInInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(TernaryInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(BreakInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(ContinueInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(ReturnInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(YieldInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(ThrowInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(AssertInstr *x) { defaultVisit(x); }
void SIRVisitor::visit(FlowInstr *x) { defaultVisit(x); }

void SIRVisitor::visit(types::PrimitiveType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::IntType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::FloatType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::BoolType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::ByteType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::VoidType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::RecordType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::RefType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::FuncType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::OptionalType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::ArrayType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::PointerType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::GeneratorType *x) { defaultVisit(x); }
void SIRVisitor::visit(types::IntNType *x) { defaultVisit(x); }

} // namespace util
} // namespace ir
} // namespace seq