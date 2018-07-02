#include "seq/seq.h"
#include "seq/exc.h"
#include "seq/count.h"

using namespace seq;
using namespace llvm;

Count::Count() : Stage("count", types::AnyType::get(), types::IntType::get())
{
}

void Count::codegen(Module *module)
{
	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();
	block = prev->getAfter();
	BasicBlock *initBlock = getEnclosingInitBlock();
	result = types::Int.storeInAlloca(getBase(), zeroLLVM(context), initBlock, true);

	IRBuilder<> builder(block);
	Value *count = builder.CreateLoad(result);
	Value *inc = builder.CreateAdd(oneLLVM(context), count);
	builder.CreateStore(inc, result);

	codegenNext(module);
	prev->setAfter(getAfter());
}

Count& Count::make()
{
	return *new Count();
}

Count *Count::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (Count *)ref->getClone(this);

	Count& x = Count::make();
	ref->addClone(this, &x);
	Stage::setCloneBase(&x, ref);
	return &x;
}
