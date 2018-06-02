#include <cstdint>
#include "seq/seq.h"
#include "seq/exc.h"
#include "seq/split.h"

using namespace seq;
using namespace llvm;

Split::Split(Expr *k, Expr *step) :
    Stage("split", types::SeqType::get(), types::SeqType::get()), k(k), step(step)
{
}

Split::Split(seq_int_t k, seq_int_t step) :
    Split(new IntExpr(k), new IntExpr(step))
{
}

void Split::codegen(Module *module)
{
	k->ensure(types::IntType::get());
	step->ensure(types::IntType::get());

	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();
	BasicBlock *entry = prev->getAfter();
	Function *func = entry->getParent();

	Value *sublen = k->codegen(getBase(), entry);
	Value *inc    = step->codegen(getBase(), entry);

	IRBuilder<> builder(entry);
	Value *seq = builder.CreateLoad(prev->result);
	Value *ptr = types::Seq.memb(seq, "ptr", entry);
	Value *len = types::Seq.memb(seq, "len", entry);
	Value *max = builder.CreateSub(len, sublen);

	BasicBlock *loop = BasicBlock::Create(context, "split", func);
	builder.CreateBr(loop);
	builder.SetInsertPoint(loop);

	PHINode *control = builder.CreatePHI(seqIntLLVM(context), 2, "i");
	Value *next = builder.CreateAdd(control, inc, "next");
	Value *cond = builder.CreateICmpSLE(control, max);

	BasicBlock *body = BasicBlock::Create(context, "body", func);
	BranchInst *branch = builder.CreateCondBr(cond, body, body);  // we set false-branch below

	block = body;
	builder.SetInsertPoint(body);
	Value *subptr = builder.CreateGEP(ptr, control);
	Value *subseq = types::Seq.make(subptr, sublen, body);
	result = types::Seq.storeInAlloca(getBase(), subseq, body, true);

	codegenNext(module);

	builder.SetInsertPoint(getAfter());
	builder.CreateBr(loop);

	control->addIncoming(ConstantInt::get(seqIntLLVM(context), 0), entry);
	control->addIncoming(next, getAfter());

	BasicBlock *exit = BasicBlock::Create(context, "exit", func);
	branch->setSuccessor(1, exit);
	prev->setAfter(exit);
}

Split& Split::make(Expr *k, Expr *step)
{
	return *new Split(k, step);
}

Split& Split::make(const seq_int_t k, const seq_int_t step)
{
	return *new Split(k, step);
}
