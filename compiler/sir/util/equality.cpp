#include "equality.h"

#include <algorithm>

#include "sir/sir.h"

#include "visitor.h"

#define VISIT(x) void visit(const x *v) override { \
                    if (!nodeId) { nodeId = &x::NodeId;  other = v;   }                \
                    if (nodeId && nodeId != &x::NodeId) result = false;                \
                    else if (v->getName() != other->getName()) result = false;         \
                    else handle(v, static_cast<const x *>(other));                                       \
                  }

namespace {
using namespace seq::ir;

class EqualityVisitor : public util::ConstIRVisitor {
private:
  const char *nodeId = nullptr;
  bool result = false;
  const IRNode *other = nullptr;
  
public:
  VISIT(Var);
  void handle(const Var *x, const Var *y) {
    result = compareVars(x, y);
  }

  VISIT(BodiedFunc);
  void handle(const BodiedFunc *x, const BodiedFunc *y) {
    result = compareFuncs(x, y) && process(x->getBody(), y->getBody());
  }
  VISIT(ExternalFunc);
  void handle(const ExternalFunc *x, const ExternalFunc *y) {
    result = x->getUnmangledName() == y->getUnmangledName() && compareFuncs(x, y);
  }
  VISIT(InternalFunc);
  void handle(const InternalFunc *x, const InternalFunc *y) {
    result = x->getParentType() == y->getParentType() && compareFuncs(x, y);
  }
  VISIT(LLVMFunc);
  void handle(const LLVMFunc *x, const LLVMFunc *y) {
    result = std::equal(x->literal_begin(), x->literal_end(), y->literal_begin(), y->literal_end())
             && x->getLLVMDeclarations() == y->getLLVMDeclarations()
             && x->getLLVMBody() == y->getLLVMBody()
             && compareFuncs(x, y);
  }

  VISIT(VarValue);
  void handle(const VarValue *x, const VarValue *y) { result = compareVars(x->getVar(), y->getVar()); }
  VISIT(PointerValue);
  void handle(const PointerValue *x, const PointerValue *y) { result = compareVars(x->getVar(), y->getVar()); }

  VISIT(SeriesFlow);
  void handle(const SeriesFlow *x, const SeriesFlow *y) {
    result = std::equal(x->begin(), x->end(), y->begin(), y->end(), process);
  }
  VISIT(IfFlow);
  void handle(const IfFlow *x, const IfFlow *y) {
    if (x->getFalseBranch() && y->getFalseBranch())
      result = process(x->getFalseBranch(), y->getFalseBranch());
    else if (!x->getFalseBranch() && !y->getFalseBranch())
      result = true;

    result = result && process(x->getCond(), y->getCond()) && process(x->getTrueBranch(), y->getTrueBranch());
  }

  VISIT(WhileFlow);
  void handle(const WhileFlow *x, const WhileFlow *y) {
    result = process(x->getCond(), y->getCond()) && process(x->getBody(), y->getBody());
  }
  VISIT(ForFlow);
  void handle(const ForFlow *x, const ForFlow *y) {}
  VISIT(TryCatchFlow);
  void handle(const TryCatchFlow *x, const TryCatchFlow *y) {}
  VISIT(PipelineFlow);
  void handle(const PipelineFlow *x, const PipelineFlow *y) {}
  VISIT(dsl::CustomFlow);
  void handle(const dsl::CustomFlow *x, const dsl::CustomFlow *y) {}

  VISIT(Constant);
  void handle(const Constant *x, const Constant *y) {}
  VISIT(IntConstant);
  void handle(const IntConstant *x, const IntConstant *y) {}
  VISIT(FloatConstant);
  void handle(const FloatConstant *x, const FloatConstant *y) {}
  VISIT(BoolConstant);
  void handle(const BoolConstant *x, const BoolConstant *y) {}
  VISIT(StringConstant);
  void handle(const StringConstant *x, const StringConstant *y) {}
  VISIT(dsl::CustomConstant);
  void handle(const dsl::CustomConstant *x, const dsl::CustomConstant *y) {}

  VISIT(Instr);
  void handle(const Instr *x, const Instr *y) {}
  VISIT(AssignInstr);
  void handle(const AssignInstr *x, const AssignInstr *y) {}
  VISIT(ExtractInstr);
  void handle(const ExtractInstr *x, const ExtractInstr *y) {}
  VISIT(InsertInstr);
  void handle(const InsertInstr *x, const InsertInstr *y) {}
  VISIT(CallInstr);
  void handle(const CallInstr *x, const CallInstr *y) {}
  VISIT(StackAllocInstr);
  void handle(const StackAllocInstr *x, const StackAllocInstr *y) {}
  VISIT(TypePropertyInstr);
  void handle(const TypePropertyInstr *x, const TypePropertyInstr *y) {}
  VISIT(YieldInInstr);
  void handle(const YieldInInstr *x, const YieldInInstr *y) {}
  VISIT(TernaryInstr);
  void handle(const TernaryInstr *x, const TernaryInstr *y) {}
  VISIT(BreakInstr);
  void handle(const BreakInstr *x, const BreakInstr *y) {}
  VISIT(ContinueInstr);
  void handle(const ContinueInstr *x, const ContinueInstr *y) {}
  VISIT(ReturnInstr);
  void handle(const ReturnInstr *x, const ReturnInstr *y) {}
  VISIT(YieldInstr);
  void handle(const YieldInstr *x, const YieldInstr *y) {}
  VISIT(ThrowInstr);
  void handle(const ThrowInstr *x, const ThrowInstr *y) {}
  VISIT(FlowInstr);
  void handle(const FlowInstr *x, const FlowInstr *y) {}
  VISIT(dsl::CustomInstr);
  void handle(const dsl::CustomInstr *x, const dsl::CustomInstr *y) {}

  VISIT(types::Type);
  void handle(const types::Type *x, const types::Type *y) {}
  VISIT(types::PrimitiveType);
  void handle(const types::PrimitiveType *x, const types::PrimitiveType *y) {}
  VISIT(types::IntType);
  void handle(const types::IntType *x, const types::IntType *y) {}
  VISIT(types::FloatType);
  void handle(const types::FloatType *x, const types::FloatType *y) {}
  VISIT(types::BoolType);
  void handle(const types::BoolType *x, const types::BoolType *y) {}
  VISIT(types::ByteType);
  void handle(const types::ByteType *x, const types::ByteType *y) {}
  VISIT(types::VoidType);
  void handle(const types::VoidType *x, const types::VoidType *y) {}
  VISIT(types::RecordType);
  void handle(const types::RecordType *x, const types::RecordType *y) {}
  VISIT(types::RefType);
  void handle(const types::RefType *x, const types::RefType *y) {}
  VISIT(types::FuncType);
  void handle(const types::FuncType *x, const types::FuncType *y) {}
  VISIT(types::OptionalType);
  void handle(const types::OptionalType *x, const types::OptionalType *y) {}
  VISIT(types::ArrayType);
  void handle(const types::ArrayType *x, const types::ArrayType *y) {}
  VISIT(types::PointerType);
  void handle(const types::PointerType *x, const types::PointerType *y) {}
  VISIT(types::GeneratorType);
  void handle(const types::GeneratorType *x, const types::GeneratorType *y) {}
  VISIT(types::IntNType);
  void handle(const types::IntNType *x, const types::IntNType *y) {}
  VISIT(dsl::types::CustomType);
  void handle(const dsl::types::CustomType *x, const dsl::types::CustomType *y) {}

  static bool process(const IRNode *x, const IRNode *y) {
    EqualityVisitor v;
    x->accept(v);
    y->accept(v);

    return v.result;
  }

private:
  static bool compareVars(const Var *x, const Var *y) {
    return process(x->getType(), y->getType());
  }

  static bool compareFuncs(const Func *x, const Func *y) {
    if (!compareVars(x, y))
      return false;

    if (!std::equal(x->arg_begin(), x->arg_end(), y->arg_begin(), y->arg_end(), process))
      return false;

    if (!std::equal(x->begin(), x->end(), y->begin(), y->arg_end(), process))
      return false;

    return true;
  }
};

}

namespace seq {
namespace ir {
namespace util {

bool areEqual(IRNode *a, IRNode *b) {

}

} // namespace util
} // namespace ir
} // namespace seq

#undef VISIT