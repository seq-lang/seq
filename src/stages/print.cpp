#include <cassert>
#include "exc.h"
#include "print.h"

using namespace seq;
using namespace llvm;

Print::Print() :
    Stage("print", types::Base::get(), types::Seq::get())
{
}

void Print::validate()
{
	if (prev)
		out = prev->getOutType();

	Stage::validate();
}

void Print::codegen(Module *module, LLVMContext& context)
{
	ensurePrev();
	validate();

	block = prev->block;
	outs->insert(prev->outs->begin(), prev->outs->end());
	prev->getOutType()->callPrint(outs, block);
	codegenNext(module, context);
	prev->setAfter(getAfter());
}

void Print::finalize(ExecutionEngine *eng)
{
	assert(prev);
	prev->getOutType()->finalizePrint(eng);
}

Print& Print::make()
{
	return *new Print();
}
