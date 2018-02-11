#include "seq.h"
#include "exc.h"
#include "count.h"

using namespace seq;
using namespace llvm;

Count::Count() : Stage("count", types::Base::get(), types::Int::get())
{
}

void Count::codegen(Module *module, LLVMContext& context)
{
	ensurePrev();
	validate();

	block = prev->block;
	IRBuilder<> preamble(getBase()->getPreamble());
	IRBuilder<> builder(block);

	Value *count = preamble.CreateAlloca(IntegerType::getInt32Ty(context),
	                                     ConstantInt::get(IntegerType::getInt32Ty(context), 1));
	preamble.CreateStore(ConstantInt::get(IntegerType::getInt32Ty(context), 0), count);

	LoadInst *load = builder.CreateLoad(count);
	Value *inc = builder.CreateAdd(builder.getInt32(1), load);
	builder.CreateStore(inc, count);

	outs->insert({SeqData::INT, inc});

	codegenNext(module, context);
	prev->setAfter(getAfter());
}

Count& Count::make()
{
	return *new Count();
}
