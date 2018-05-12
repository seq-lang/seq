#include "seq/common.h"
#include "seq/cell.h"

using namespace seq;
using namespace llvm;

Cell::Cell(BaseFunc *base, Expr *init) :
    base(base), init(init), ptr(nullptr)
{
}

void Cell::codegen(BasicBlock *block)
{
	assert(!ptr);
	LLVMContext& context = block->getContext();
	IRBuilder<> builder(block);
	ptr = makeAlloca(getType()->getLLVMType(context), base->getPreamble());
	Value *val = init->codegen(base, block);
	builder.CreateStore(val, ptr);
}

Value *Cell::load(BasicBlock *block)
{
	assert(ptr);
	IRBuilder<> builder(block);
	return builder.CreateLoad(ptr);
}

void Cell::store(Value *val, BasicBlock *block)
{
	assert(ptr);
	IRBuilder<> builder(block);
	builder.CreateStore(val, ptr);
}

types::Type *Cell::getType()
{
	return init->getType();
}
