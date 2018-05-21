#include <cstdint>
#include <string>
#include "seq/seq.h"
#include "seq/exc.h"
#include "seq/range.h"

using namespace seq;
using namespace llvm;

Range::Range(seq_int_t from, seq_int_t to, seq_int_t step) :
    Stage("range", types::AnyType::get(), types::IntType::get()),
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

void Range::codegen(Module *module)
{
	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();
	BasicBlock *preambleBlock = getBase()->getPreamble();
	BasicBlock *entry = prev->getAfter();
	Function *func = entry->getParent();

	Value *from = ConstantInt::get(seqIntLLVM(context), (uint64_t)this->from, true);
	Value *to   = ConstantInt::get(seqIntLLVM(context), (uint64_t)this->to,   true);
	Value *step = ConstantInt::get(seqIntLLVM(context), (uint64_t)this->step, true);

	BasicBlock *loop = BasicBlock::Create(context, "range", func);
	IRBuilder<> builder(entry);
	builder.CreateBr(loop);
	builder.SetInsertPoint(loop);

	PHINode *control = builder.CreatePHI(seqIntLLVM(context), 2, "i");
	Value *next = builder.CreateAdd(control, step, "next");
	Value *cond = builder.CreateICmpSLT(control, to);

	Value *intVar = makeAlloca(zeroLLVM(context), preambleBlock);
	builder.CreateStore(control, intVar);
	outs->insert({SeqData::INT, intVar});

	BasicBlock *body = BasicBlock::Create(context, "body", func);
	BranchInst *branch = builder.CreateCondBr(cond, body, body);  // we set false-branch below

	block = body;

	codegenNext(module);

	builder.SetInsertPoint(getAfter());
	builder.CreateBr(loop);

	control->addIncoming(from, entry);
	control->addIncoming(next, getAfter());

	BasicBlock *exit = BasicBlock::Create(context, "exit", func);
	branch->setSuccessor(1, exit);
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
