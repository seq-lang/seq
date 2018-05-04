#include "seq/seq.h"
#include "seq/exc.h"
#include "seq/count.h"

using namespace seq;
using namespace llvm;

Count::Count() : Stage("count", types::BaseType::get(), types::IntType::get())
{
}

void Count::codegen(Module *module)
{
	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();
	block = prev->getAfter();
	BasicBlock *preambleBlock = getBase()->getPreamble();
	IRBuilder<> builder(block);

	Value *countVar = makeAlloca(zeroLLVM(context), preambleBlock);

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
