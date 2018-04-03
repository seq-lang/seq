#include "func.h"
#include "call.h"

using namespace seq;
using namespace llvm;

static types::Type *voidToAny(types::Type *type)
{
	if (type->isChildOf(types::VoidType::get()))
		return types::AnyType::get();
	return type;
}

Call::Call(Func& func) :
    Stage("call", voidToAny(func.getInType()), func.getOutType()), func(func)
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

void Call::finalize(ExecutionEngine *eng)
{
	func.finalize(eng);
}

Call& Call::make(Func& func)
{
	return *new Call(func);
}
