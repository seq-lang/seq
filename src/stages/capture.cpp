#include "seq/capture.h"

using namespace seq;
using namespace llvm;

Capture::Capture(void *addr) : Stage("capture"), addr(addr)
{
}

void Capture::validate()
{
	if (prev)
		in = prev->getOutType();

	Stage::validate();
}

void Capture::codegen(Module *module)
{
	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();

	block = prev->getAfter();
	IRBuilder<> builder(block);

	Value *addrAsInt = ConstantInt::get(IntegerType::getIntNTy(context, sizeof(void *) * 8), (uint64_t)addr);
	Value *addrAsPtr = builder.CreateIntToPtr(addrAsInt, PointerType::get(getInType()->getLLVMType(context), 0));
	Value *val = builder.CreateLoad(prev->result);
	builder.CreateStore(val, addrAsPtr);

	codegenNext(module);
	prev->setAfter(getAfter());
}

Capture& Capture::make(void *addr)
{
	return *new Capture(addr);
}
