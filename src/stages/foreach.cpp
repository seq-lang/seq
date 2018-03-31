#include <cassert>
#include "foreach.h"

using namespace seq;
using namespace llvm;

ForEach::ForEach() : Stage("foreach", types::ArrayType::get(), types::VoidType::get())
{
}

void ForEach::validate()
{
	if (prev && prev->getOutType()->isChildOf(types::ArrayType::get())) {
		auto *type = dynamic_cast<types::ArrayType *>(prev->getOutType());
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
	Value *ptrVar = getSafe(prev->outs, SeqData::ARRAY);
	Value *lenVar = getSafe(prev->outs, SeqData::LEN);

	BasicBlock *entry = prev->block;
	Function *func = entry->getParent();

	BasicBlock *loop = BasicBlock::Create(context, "foreach", func);
	IRBuilder<> builder(entry);
	builder.CreateBr(loop);
	builder.SetInsertPoint(loop);

	PHINode *control = builder.CreatePHI(seqIntLLVM(context), 2, "i");

	auto *type = dynamic_cast<types::ArrayType *>(getInType());
	assert(type != nullptr);

	block = loop;
	builder.SetInsertPoint(block);
	Value *ptr = builder.CreateLoad(ptrVar);
	Value *len = builder.CreateLoad(lenVar);

	type->getBaseType()->codegenLoad(getBase(),
	                                 outs,
	                                 block,
	                                 ptr,
	                                 control);

	codegenNext(module);

	builder.SetInsertPoint(getAfter());
	Value *next = builder.CreateAdd(control, ConstantInt::get(seqIntLLVM(context), 1), "next");

	control->addIncoming(ConstantInt::get(seqIntLLVM(context), 0), entry);
	control->addIncoming(next, getAfter());

	BasicBlock *exit = BasicBlock::Create(context, "exit", func);
	Value *cond = builder.CreateICmpSLT(next, len);
	builder.CreateCondBr(cond, loop, exit);

	prev->setAfter(exit);
}

ForEach& ForEach::make()
{
	return *new ForEach();
}
