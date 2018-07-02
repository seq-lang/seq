#include <string>
#include <vector>
#include "seq/seq.h"
#include "seq/opstage.h"

using namespace seq;
using namespace llvm;

OpStage::OpStage(std::string name, SeqOp op) :
    Stage(std::move(name), types::SeqType::get(), types::SeqType::get()), op(op)
{
}

void OpStage::codegen(Module *module)
{
	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();
	func = cast<Function>(
	         module->getOrInsertFunction(
	           name,
	           Type::getVoidTy(context),
	           IntegerType::getInt8PtrTy(context),
	           seqIntLLVM(context)));

	func->setCallingConv(CallingConv::C);

	block = prev->getAfter();
	result = prev->result;
	IRBuilder<> builder(block);
	Value *seq = builder.CreateLoad(result);
	Value *ptr = types::Seq.memb(seq, "ptr", block);
	Value *len = types::Seq.memb(seq, "len", block);
	builder.CreateCall(func, {ptr, len});

	codegenNext(module);
	prev->setAfter(getAfter());
}

void OpStage::finalize(Module *module, ExecutionEngine *eng)
{
	eng->addGlobalMapping(func, (void *)op);
	Stage::finalize(module, eng);
}

OpStage& OpStage::make(std::string name, SeqOp op)
{
	return *new OpStage(std::move(name), op);
}

OpStage *OpStage::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (OpStage *)ref->getClone(this);

	OpStage& x = OpStage::make(name, op);
	ref->addClone(this, &x);
	Stage::setCloneBase(&x, ref);
	return &x;
}
