#include "len.h"

using namespace seq;
using namespace llvm;

Len::Len() : Stage("len", types::Base::get(), types::Int::get())
{
}

void Len::codegen(llvm::Module *module, llvm::LLVMContext &context)
{
	ensurePrev();
	validate();

	if (!prev || !prev->block)
		throw exc::StageException("previous stage not compiled", *this);

	auto leniter = prev->outs->find(SeqData::LEN);

	if (leniter == outs->end())
		throw exc::StageException("pipeline error", *this);

	block = prev->block;
	Value *len = leniter->second;
	outs->insert({SeqData::INT, len});
	codegenNext(module, context);
	prev->setAfter(getAfter());
}

Len& Len::make()
{
	return *new Len();
}
