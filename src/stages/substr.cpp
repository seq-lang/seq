#include <cstdint>
#include "exc.h"
#include "substr.h"

using namespace llvm;
using namespace seq;

Substr::Substr(uint32_t start, uint32_t len) :
    Stage("split", types::Seq::get(), types::Seq::get()), start(start - 1), len(len)
{
}

void Substr::codegen(Module *module, LLVMContext& context)
{
	ensurePrev();
	validate();

	auto seqiter = prev->outs->find(SeqData::SEQ);

	if (seqiter == outs->end())
		throw exc::StageException("pipeline error", *this);

	Value *seq = seqiter->second;
	block = prev->block;
	IRBuilder<> builder(block);

	Value *subidx  = ConstantInt::get(Type::getInt32Ty(context), start);
	Value *subseq = builder.CreateGEP(seq, subidx);
	Value *sublen = ConstantInt::get(Type::getInt32Ty(context), len);

	outs->insert({SeqData::SEQ, subseq});
	outs->insert({SeqData::LEN, sublen});

	codegenNext(module, context);
	prev->setAfter(getAfter());
}

Substr& Substr::make(const uint32_t start, const uint32_t len)
{
	return *new Substr(start, len);
}
