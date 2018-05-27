#include "seq/seq.h"
#include "seq/exc.h"
#include "seq/copy.h"

using namespace seq;
using namespace llvm;

Copy::Copy() : Stage("copy")
{
}

void Copy::validate()
{
	if (prev)
		in = out = prev->getOutType();

	Stage::validate();
}

void Copy::codegen(Module *module)
{
	ensurePrev();
	validate();

	block = prev->getAfter();
	getInType()->callCopy(getBase(), prev->outs, outs, block);

	codegenNext(module);
	prev->setAfter(getAfter());
}

void Copy::finalize(Module *module, ExecutionEngine *eng)
{
	getInType()->finalizeCopy(module, eng);
}

Copy& Copy::make()
{
	return *new Copy();
}
