#include <cassert>
#include "exc.h"
#include "print.h"

using namespace seq;
using namespace llvm;

Print::Print() :
    Stage("print", types::BaseType::get(), types::SeqType::get())
{
}

void Print::validate()
{
	if (prev)
		out = prev->getOutType();

	Stage::validate();
}

void Print::codegen(Module *module)
{
	ensurePrev();
	validate();

	block = prev->block;
	outs->insert(prev->outs->begin(), prev->outs->end());
	prev->getOutType()->callPrint(getBase(), outs, block);
	codegenNext(module);
	prev->setAfter(getAfter());
}

void Print::finalize(ExecutionEngine *eng)
{
	assert(prev);
	prev->getOutType()->finalizePrint(eng);
	Stage::finalize(eng);
}

Print& Print::make()
{
	return *new Print();
}
