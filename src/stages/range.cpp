#include <cstdint>
#include <string>
#include "seq/seq.h"
#include "seq/exc.h"
#include "seq/range.h"

using namespace seq;
using namespace llvm;

Range::Range(Expr *from, Expr *to, Expr *step) :
    Stage("range", types::AnyType::get(), types::IntType::get()),
    from(from), to(to), step(step)
{
	loop = true;
}

Range::Range(Expr *from, Expr *to) :
    Range(from, to, new IntExpr(1))
{
}

Range::Range(Expr *to) :
    Range(new IntExpr(0), to, new IntExpr(1))
{
}

Range::Range(seq_int_t from, seq_int_t to, seq_int_t step) :
    Range(new IntExpr(from), new IntExpr(to), new IntExpr(step))
{
}

Range::Range(seq_int_t from, seq_int_t to) :
    Range(new IntExpr(from), new IntExpr(to))
{
}

Range::Range(seq_int_t to) :
    Range(new IntExpr(to))
{
}

void Range::codegen(Module *module)
{
	from->ensure(types::IntType::get());
	to->ensure(types::IntType::get());
	step->ensure(types::IntType::get());

	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();
	BasicBlock *entry = prev->getAfter();
	Function *func = entry->getParent();

	Value *from = this->from->codegen(getBase(), entry);
	Value *to   = this->to->codegen(getBase(), entry);
	Value *step = this->step->codegen(getBase(), entry);

	BasicBlock *loop = BasicBlock::Create(context, "range", func);
	IRBuilder<> builder(entry);
	builder.CreateBr(loop);
	builder.SetInsertPoint(loop);

	PHINode *control = builder.CreatePHI(seqIntLLVM(context), 2, "i");
	Value *next = builder.CreateAdd(control, step, "next");
	Value *cond = builder.CreateICmpSLT(control, to);

	result = types::Int.storeInAlloca(getBase(), control, loop, true);

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

	setBreaks(exit);
	setContinues(loop);
}

Range& Range::make(Expr *from, Expr *to, Expr *step)
{
	return *new Range(from, to, step);
}

Range& Range::make(Expr *from, Expr *to)
{
	return *new Range(from, to);
}

Range& Range::make(Expr *to)
{
	return *new Range(to);
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
