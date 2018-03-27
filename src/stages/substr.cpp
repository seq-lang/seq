#include <cstdint>
#include "exc.h"
#include "substr.h"

using namespace llvm;
using namespace seq;

Substr::Substr(seq_int_t start, seq_int_t len) :
    Stage("split", types::SeqType::get(), types::SeqType::get()), start(start - 1), len(len)
{
	name += "(" + std::to_string(start) + "," + std::to_string(len) + ")";
}

void Substr::codegen(Module *module)
{
	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();
	Value *seq = getSafe(prev->outs, SeqData::SEQ);
	block = prev->block;
	IRBuilder<> builder(block);

	Value *subidx  = ConstantInt::get(seqIntLLVM(context), (uint64_t)start);
	Value *subseq = builder.CreateGEP(seq, subidx);
	Value *sublen = ConstantInt::get(seqIntLLVM(context), (uint64_t)len);

	outs->insert({SeqData::SEQ, subseq});
	outs->insert({SeqData::LEN, sublen});

	codegenNext(module);
	prev->setAfter(getAfter());
}

Substr& Substr::make(const seq_int_t start, const seq_int_t len)
{
	return *new Substr(start, len);
}
