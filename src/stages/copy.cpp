#include "exc.h"
#include "copy.h"

using namespace seq;
using namespace llvm;

Copy::Copy() : Stage("copy", types::Seq(), types::Seq())
{
}

void Copy::codegen(Module *module, LLVMContext& context)
{
	auto seqiter = prev->outs->find(SeqData::SEQ);
	auto leniter = prev->outs->find(SeqData::LEN);

	if (seqiter == outs->end() || leniter == outs->end())
		throw exc::StageException("pipeline error", *this);

	Value *seq = seqiter->second;
	Value *len = leniter->second;
	block = prev->block;
	IRBuilder<> builder(block);

	Value *copy = builder.CreateAlloca(IntegerType::getInt8Ty(context), len);
	builder.CreateMemCpy(copy, seq, len, 1);

	outs->insert({SeqData::SEQ, copy});
	outs->insert({SeqData::LEN, len});

	codegenNext(module, context);
	prev->setAfter(getAfter());
}

Copy& Copy::make()
{
	return *new Copy();
}
