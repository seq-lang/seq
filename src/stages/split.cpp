#include <cstdint>
#include "exc.h"
#include "split.h"

using namespace seq;
using namespace llvm;

Split::Split(seq_int_t k, seq_int_t step) :
    Stage("split", types::SeqType::get(), types::SeqType::get()), k(k), step(step)
{
	name += "(" + std::to_string(k) + "," + std::to_string(step) + ")";
}

void Split::codegen(Module *module)
{
	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();
	auto seqiter = prev->outs->find(SeqData::SEQ);
	auto leniter = prev->outs->find(SeqData::LEN);

	if (seqiter == prev->outs->end() || leniter == prev->outs->end())
		throw exc::StageException("pipeline error", *this);

	Value *seq = seqiter->second;
	Value *len = leniter->second;

	BasicBlock *entry = prev->block;
	Function *func = entry->getParent();

	BasicBlock *loop = BasicBlock::Create(context, "loop", func);
	IRBuilder<> builder(entry);
	builder.CreateBr(loop);
	builder.SetInsertPoint(loop);

	PHINode *control = builder.CreatePHI(seqIntLLVM(context), 2, "i");

	Value *subseq = builder.CreateGEP(seq, control);
	Value *sublen = ConstantInt::get(seqIntLLVM(context), (uint64_t)k);

	outs->insert({SeqData::SEQ, subseq});
	outs->insert({SeqData::LEN, sublen});
	block = loop;

	codegenNext(module);

	builder.SetInsertPoint(getAfter());
	Value *inc = ConstantInt::get(seqIntLLVM(context), (uint64_t)step);
	Value *next = builder.CreateAdd(control, inc, "next");

	control->addIncoming(ConstantInt::get(seqIntLLVM(context), 0), entry);
	control->addIncoming(next, getAfter());

	BasicBlock *exit = BasicBlock::Create(context, "exit", func);
	Value *max = builder.CreateSub(len, sublen);
	Value *cond = builder.CreateICmpSLE(next, max);
	builder.CreateCondBr(cond, loop, exit);

	prev->setAfter(exit);
}

Split& Split::make(const seq_int_t k, const seq_int_t step)
{
	return *new Split(k, step);
}
