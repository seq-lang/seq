#include "len.h"

using namespace seq;
using namespace llvm;

Len::Len() : Stage("len", types::BaseType::get(), types::IntType::get())
{
}

void Len::codegen(llvm::Module *module)
{
	ensurePrev();
	validate();

	auto leniter = prev->outs->find(SeqData::LEN);

	if (leniter == prev->outs->end())
		throw exc::StageException("pipeline error", *this);

	block = prev->block;
	Value *len = leniter->second;
	outs->insert({SeqData::INT, len});
	codegenNext(module);
	prev->setAfter(getAfter());
}

Len& Len::make()
{
	return *new Len();
}
