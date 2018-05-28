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
	BasicBlock *preambleBlock = getBase()->getPreamble();
	BasicBlock *initBlock = getEnclosingInitBlock();

	Value *countVar = makeAlloca(zeroLLVM(context), preambleBlock);

	IRBuilder<> builder(initBlock);
	builder.CreateStore(zeroLLVM(context), countVar);

	builder.SetInsertPoint(block);
	Value *count = builder.CreateLoad(countVar);
	Value *inc = builder.CreateAdd(oneLLVM(context), count);
	builder.CreateStore(inc, countVar);

	outs->insert({SeqData::INT, countVar});

	codegenNext(module);
	prev->setAfter(getAfter());
}

Count& Count::make()
{
	return *new Count();
}
