#include <cstdint>
#include "exc.h"
#include "split.h"

using namespace seq;
using namespace llvm;

Split::Split(uint32_t k, uint32_t step) :
    Stage("split", types::Seq::get(), types::Seq::get()), k(k), step(step)
{
}

void Split::codegen(Module *module, LLVMContext& context)
{
	ensurePrev();
	validate();

	auto seqiter = prev->outs->find(SeqData::SEQ);
	auto leniter = prev->outs->find(SeqData::LEN);

	if (seqiter == outs->end() || leniter == outs->end())
		throw exc::StageException("pipeline error", *this);

	Value *seq = seqiter->second;
	Value *len = leniter->second;

	BasicBlock *entry = prev->block;
	Function *func = entry->getParent();

	BasicBlock *loop = BasicBlock::Create(context, "loop", func);
	IRBuilder<> builder(entry);
	builder.CreateBr(loop);
	builder.SetInsertPoint(loop);

	PHINode *control = builder.CreatePHI(IntegerType::getInt32Ty(context), 2, "i");

	Value *subseq = builder.CreateGEP(seq, control);
	Value *sublen = ConstantInt::get(Type::getInt32Ty(context), k);

	outs->insert({SeqData::SEQ, subseq});
	outs->insert({SeqData::LEN, sublen});
	block = loop;

	codegenNext(module, context);

	builder.SetInsertPoint(getAfter());
	Value *inc = ConstantInt::get(Type::getInt32Ty(context), step);
	Value *next = builder.CreateAdd(control, inc, "next");

	control->addIncoming(ConstantInt::get(Type::getInt32Ty(context), 0), entry);
	control->addIncoming(next, getAfter());

	BasicBlock *exit = BasicBlock::Create(context, "exit", func);
	Value *max = builder.CreateSub(len, sublen);
	Value *cond = builder.CreateICmpULE(next, max);
	builder.CreateCondBr(cond, loop, exit);

	prev->setAfter(exit);
}

Split& Split::make(const uint32_t k, const uint32_t step)
{
	return *new Split(k, step);
}
