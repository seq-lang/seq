#include <cassert>
#include "seq/exc.h"
#include "seq/print.h"

using namespace seq;
using namespace llvm;

Print::Print() :
    Stage("print", types::BaseType::get(), types::VoidType::get())
{
}

void Print::validate()
{
	if (prev)
		in = out = prev->getOutType();

	Stage::validate();
}

void Print::codegen(Module *module)
{
	ensurePrev();
	validate();

	block = prev->getAfter();
	outs->insert(prev->outs->begin(), prev->outs->end());
	prev->getOutType()->callPrint(getBase(), outs, block);
	codegenNext(module);
	prev->setAfter(getAfter());
}

void Print::finalize(Module *module, ExecutionEngine *eng)
{
	assert(prev);
	prev->getOutType()->finalizePrint(module, eng);
	Stage::finalize(module, eng);
}

Print& Print::make()
{
	return *new Print();
}
