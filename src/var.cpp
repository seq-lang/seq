#include "seq/common.h"
#include "seq/var.h"

using namespace seq;
using namespace llvm;

Var::Var(types::Type *type) :
    type(type), ptr(nullptr)
{
}

void Var::allocaIfNeeded(BaseFunc *base)
{
	if (ptr)
		return;

	assert(type);
	LLVMContext& context = base->getContext();
	ptr = makeAlloca(getType()->getLLVMType(context), base->getPreamble());
}

Value *Var::load(BaseFunc *base, BasicBlock *block)
{
	allocaIfNeeded(base);
	IRBuilder<> builder(block);
	return builder.CreateLoad(ptr);
}

void Var::store(BaseFunc *base, Value *val, BasicBlock *block)
{
	allocaIfNeeded(base);
	IRBuilder<> builder(block);
	builder.CreateStore(val, ptr);
}

void Var::setType(types::Type *type)
{
	assert(!this->type);
	this->type = type;
}

types::Type *Var::getType()
{
	assert(type);
	return type;
}

Var *Var::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (Var *)ref->getClone(this);

	auto *x = new Var();
	ref->addClone(this, x);
	if (type) x->setType(type->clone(ref));
	return x;
}
