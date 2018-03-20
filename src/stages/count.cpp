#include "seq.h"
#include "exc.h"
#include "count.h"

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
	block = prev->block;
	IRBuilder<> preamble(getBase()->getPreamble());
	IRBuilder<> builder(block);

	Value *count = preamble.CreateAlloca(seqIntLLVM(context),
	                                     ConstantInt::get(seqIntLLVM(context), 1));
	preamble.CreateStore(ConstantInt::get(seqIntLLVM(context), 0), count);

	LoadInst *load = builder.CreateLoad(count);
	Value *inc = builder.CreateAdd(ConstantInt::get(seqIntLLVM(context), 1), load);
	builder.CreateStore(inc, count);

	outs->insert({SeqData::INT, inc});

	codegenNext(module);
	prev->setAfter(getAfter());
}

Count& Count::make()
{
	return *new Count();
}
