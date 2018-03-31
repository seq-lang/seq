#include <cstdint>
#include "seq.h"
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
	BasicBlock *preambleBlock = getBase()->getPreamble();

	block = prev->block;
	IRBuilder<> builder(block);

	Value *seq = builder.CreateLoad(getSafe(prev->outs, SeqData::SEQ));
	Value *subidx  = ConstantInt::get(seqIntLLVM(context), (uint64_t)start);
	Value *subseq = builder.CreateGEP(seq, subidx);
	Value *sublen = ConstantInt::get(seqIntLLVM(context), (uint64_t)len);

	Value *subseqVar = makeAlloca(nullPtrLLVM(context), preambleBlock);
	Value *sublenVar = makeAlloca(zeroLLVM(context), preambleBlock);

	builder.CreateStore(subseq, subseqVar);
	builder.CreateStore(sublen, sublenVar);

	outs->insert({SeqData::SEQ, subseqVar});
	outs->insert({SeqData::LEN, sublenVar});

	codegenNext(module);
	prev->setAfter(getAfter());
}

Substr& Substr::make(const seq_int_t start, const seq_int_t len)
{
	return *new Substr(start, len);
}
