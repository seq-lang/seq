#include "len.h"

using namespace seq;
using namespace llvm;

Len::Len() : Stage("len", types::BaseType::get(), types::IntType::get())
{
}

void Len::codegen(Module *module)
{
	ensurePrev();
	validate();

	block = prev->block;
	outs->insert({SeqData::INT, getSafe(prev->outs, SeqData::LEN)});
	codegenNext(module);
	prev->setAfter(getAfter());
}

Len& Len::make()
{
	return *new Len();
}
