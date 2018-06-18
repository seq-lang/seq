#include "seq/seq.h"
#include "seq/len.h"

using namespace seq;
using namespace llvm;

Len::Len() : Stage("len", types::BaseType::get(), types::IntType::get())
{
}

void Len::codegen(Module *module)
{
	ensurePrev();
	validate();

	block = prev->getAfter();
	IRBuilder<> builder(block);
	Value *val = builder.CreateLoad(prev->result);
	Value *len = prev->getOutType()->memb(val, "len", block);
	result = types::Int.storeInAlloca(getBase(), len, block, true);

	codegenNext(module);
	prev->setAfter(getAfter());
}

Len& Len::make()
{
	return *new Len();
}

Len *Len::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (Len *)ref->getClone(this);

	Len& x = Len::make();
	ref->addClone(this, &x);
	Stage::setCloneBase(&x, ref);
	return &x;
}
