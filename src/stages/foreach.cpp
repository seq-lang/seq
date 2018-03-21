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
	auto ptriter = prev->outs->find(SeqData::ARRAY);
	auto leniter = prev->outs->find(SeqData::LEN);

	if (ptriter == prev->outs->end() || leniter == prev->outs->end())
		throw exc::StageException("pipeline error", *this);

	Value *ptr = ptriter->second;
	Value *len = leniter->second;

	BasicBlock *entry = prev->block;
	Function *func = entry->getParent();

	BasicBlock *loop = BasicBlock::Create(context, "foreach", func);
	IRBuilder<> builder(entry);
	builder.CreateBr(loop);
	builder.SetInsertPoint(loop);

	PHINode *control = builder.CreatePHI(seqIntLLVM(context), 2, "i");
	Value *ptrActual = builder.CreateLoad(ptr);
	Value *val = builder.CreateGEP(ptrActual, control);

	outs->insert({out->getKey(), builder.CreateLoad(val)});
	block = loop;

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
