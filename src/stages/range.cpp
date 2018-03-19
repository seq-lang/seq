#include <cstdint>
#include <string>
#include "exc.h"
#include "range.h"

using namespace seq;
using namespace llvm;

Range::Range(seq_int_t from, seq_int_t to, seq_int_t step) :
    Stage("range", types::BaseType::get(), types::IntType::get()),
    from(from), to(to), step(step)
{
	if (from > to)
		throw exc::StageException("invalid range boundaries", *this);

	name += "(" + std::to_string(from) + "," + std::to_string(to) + "," + std::to_string(step) + ")";
}

Range::Range(seq_int_t from, seq_int_t to) :
    Range(from, to, 1)
{
}

Range::Range(seq_int_t to) :
    Range(0, to, 1)
{
}

void Range::codegen(Module *module, LLVMContext& context)
{
	ensurePrev();
	validate();

	Value *from = ConstantInt::get(seqIntLLVM(context), (uint64_t)this->from);
	Value *to   = ConstantInt::get(seqIntLLVM(context), (uint64_t)this->to);
	Value *step = ConstantInt::get(seqIntLLVM(context), (uint64_t)this->step);

	BasicBlock *entry = prev->block;
	Function *func = entry->getParent();

	BasicBlock *loop = BasicBlock::Create(context, "range", func);
	IRBuilder<> builder(entry);
	builder.CreateBr(loop);
	builder.SetInsertPoint(loop);

	PHINode *control = builder.CreatePHI(seqIntLLVM(context), 2, "i");

	outs->insert({SeqData::INT, control});
	block = loop;

	codegenNext(module, context);

	builder.SetInsertPoint(getAfter());
	Value *next = builder.CreateAdd(control, step, "next");

	control->addIncoming(from, entry);
	control->addIncoming(next, getAfter());

	BasicBlock *exit = BasicBlock::Create(context, "exit", func);
	Value *cond = builder.CreateICmpULT(next, to);
	builder.CreateCondBr(cond, loop, exit);

	prev->setAfter(exit);
}

Range& Range::make(seq_int_t from, seq_int_t to, seq_int_t step)
{
	return *new Range(from, to, step);
}

Range& Range::make(seq_int_t from, seq_int_t to)
{
	return *new Range(from, to);
}

Range& Range::make(seq_int_t to)
{
	return *new Range(to);
}
