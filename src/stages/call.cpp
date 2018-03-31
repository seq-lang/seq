#include "func.h"
#include "call.h"

using namespace seq;
using namespace llvm;

Call::Call(BaseFunc& func) :
    Stage("call", func.getInType(), func.getOutType()), func(func)
{
}

void Call::codegen(Module *module)
{
	ensurePrev();
	validate();

	block = prev->block;
	func.codegenCall(getBase(), prev->outs, outs, block);
	codegenNext(module);
	prev->setAfter(getAfter());
}

Call& Call::make(BaseFunc& func)
{
	return *new Call(func);
}
