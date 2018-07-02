#include <cstdint>
#include "seq/seq.h"
#include "seq/exc.h"
#include "seq/substr.h"

using namespace llvm;
using namespace seq;

Substr::Substr(Expr *start, Expr *len) :
    Stage("substr", types::SeqType::get(), types::SeqType::get()), start(start), len(len)
{
}

Substr::Substr(seq::seq_int_t k, seq::seq_int_t step) :
    Substr(new IntExpr(k), new IntExpr(step))
{
}

void Substr::codegen(Module *module)
{
	start->ensure(types::IntType::get());
	len->ensure(types::IntType::get());

	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();
	block = prev->getAfter();

	Value *subidx = start->codegen(getBase(), block);
	Value *sublen = len->codegen(getBase(), block);

	IRBuilder<> builder(block);
	subidx = builder.CreateSub(subidx, oneLLVM(context));

	Value *seq = builder.CreateLoad(prev->result);
	Value *ptr = types::Seq.memb(seq, "ptr", block);
	Value *subptr = builder.CreateGEP(ptr, subidx);
	Value *subseq = types::Seq.make(subptr, sublen, block);
	result = types::Seq.storeInAlloca(getBase(), subseq, block, true);

	codegenNext(module);
	prev->setAfter(getAfter());
}

Substr& Substr::make(Expr *start, Expr *len)
{
	return *new Substr(start, len);
}

Substr& Substr::make(const seq_int_t start, const seq_int_t len)
{
	return *new Substr(start, len);
}

Substr *Substr::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (Substr *)ref->getClone(this);

	Substr& x = Substr::make(start->clone(ref), len->clone(ref));
	ref->addClone(this, &x);
	Stage::setCloneBase(&x, ref);
	return &x;
}
