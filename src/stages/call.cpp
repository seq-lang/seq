#include "seq/record.h"
#include "seq/func.h"
#include "seq/call.h"

using namespace seq;
using namespace llvm;

static types::Type *voidToAny(types::Type *type)
{
	if (type->is(types::VoidType::get()))
		return types::AnyType::get();
	return type;
}

Call::Call(Func& func) :
    Stage("call", voidToAny(func.getInType()), func.getOutType()), func(func)
{
}

void Call::validate()
{
	if (!func.singleInput())
		throw exc::SeqException("pipelined function must be single-input");

	Stage::validate();
}

void Call::codegen(Module *module)
{
	ensurePrev();
	validate();

	bool voidInput = func.getInType()->is(types::VoidType::get());
	block = prev->getAfter();

	Value *val = nullptr;
	if (voidInput) {
		val = func.codegenCall(getBase(), {}, block);
	} else {
		IRBuilder<> builder(block);
		Value *arg = builder.CreateLoad(prev->result);
		val = func.codegenCall(getBase(), {arg}, block);
	}
	result = func.getOutType()->storeInAlloca(getBase(), val, block, true);

	codegenNext(module);
	prev->setAfter(getAfter());
}

void Call::finalize(Module *module, ExecutionEngine *eng)
{
	func.finalize(module, eng);
	Stage::finalize(module, eng);
}

Call& Call::make(Func& func)
{
	return *new Call(func);
}
