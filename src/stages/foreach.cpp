#include <cassert>
#include "seq/foreach.h"

using namespace seq;
using namespace llvm;

ForEach::ForEach() : Stage("foreach", types::ArrayType::get(), types::VoidType::get())
{
	loop = true;
}

void ForEach::validate()
{
	if (getPrev() && getPrev()->getOutType()->isGeneric(types::ArrayType::get())) {
		auto *type = dynamic_cast<types::ArrayType *>(getPrev()->getOutType());
		assert(type != nullptr);
		in = type;
		out = type->getBaseType();

		if (out->getKey() == SeqData::NONE)
			throw exc::SeqException("cannot iterate over array of '" + out->getName() + "'");
	}

	Stage::validate();
}

void ForEach::codegen(Module *module)
{
	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();

	BasicBlock *entry = prev->getAfter();
	Function *func = entry->getParent();

	IRBuilder<> builder(entry);
	Value *arr = builder.CreateLoad(prev->result);
	Value *len = getInType()->memb(arr, "len", entry);

	BasicBlock *loopCont = BasicBlock::Create(context, "foreach_cont", func);
	BasicBlock *loop = BasicBlock::Create(context, "foreach", func);
	builder.CreateBr(loop);

	builder.SetInsertPoint(loopCont);
	builder.CreateBr(loop);

	builder.SetInsertPoint(loop);
	PHINode *control = builder.CreatePHI(seqIntLLVM(context), 3, "i");
	Value *cond = builder.CreateICmpSLT(control, len);
	Value *next = builder.CreateAdd(control, oneLLVM(context), "next");

	BasicBlock *body = BasicBlock::Create(context, "body", func);
	BranchInst *branch = builder.CreateCondBr(cond, body, body);  // we set false-branch below

	auto *type = dynamic_cast<types::ArrayType *>(getInType());
	assert(type != nullptr);

	block = body;
	Value *val = type->indexLoad(getBase(), arr, control, block);
	result = type->getBaseType()->storeInAlloca(getBase(), val, block, true);

	codegenNext(module);

	builder.SetInsertPoint(getAfter());
	builder.CreateBr(loop);

	control->addIncoming(zeroLLVM(context), entry);
	control->addIncoming(next, loopCont);
	control->addIncoming(next, getAfter());

	BasicBlock *exit = BasicBlock::Create(context, "exit", func);
	branch->setSuccessor(1, exit);
	prev->setAfter(exit);

	setBreaks(exit);
	setContinues(loopCont);
}

ForEach& ForEach::make()
{
	return *new ForEach();
}
