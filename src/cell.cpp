#include "seq/common.h"
#include "seq/cell.h"

using namespace seq;
using namespace llvm;

Cell::Cell(types::Type *type) :
    type(type), ptr(nullptr)
{
}

void Cell::allocaIfNeeded(BaseFunc *base)
{
	if (ptr)
		return;

	assert(type);
	LLVMContext& context = base->getContext();
	ptr = makeAlloca(getType()->getLLVMType(context), base->getPreamble());
}

Value *Cell::load(BaseFunc *base, BasicBlock *block)
{
	allocaIfNeeded(base);
	IRBuilder<> builder(block);
	return builder.CreateLoad(ptr);
}

void Cell::store(BaseFunc *base, Value *val, BasicBlock *block)
{
	allocaIfNeeded(base);
	IRBuilder<> builder(block);
	builder.CreateStore(val, ptr);
}

void Cell::setType(types::Type *type)
{
	assert(!this->type);
	this->type = type;
}

types::Type *Cell::getType()
{
	assert(type);
	return type;
}

Cell *Cell::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (Cell *)ref->getClone(this);

	auto *x = new Cell();
	ref->addClone(this, x);
	if (type) x->setType(type->clone(ref));
	return x;
}
