#include "seq/seq.h"
#include "seq/util.h"
#include "seq/collect.h"

using namespace seq;
using namespace llvm;

static const size_t INIT_VEC_SIZE = 10;

Collect::Collect() :
    Stage("collect"), appendFunc(nullptr)
{
}

void Collect::validate()
{
	if (prev) {
		in = prev->getOutType();
		out = types::ArrayType::get(in);
	}

	Stage::validate();
}

void Collect::codegen(Module *module)
{
	ensurePrev();
	validate();

	if (getInType()->getKey() == SeqData::NONE || getInType()->size(module) == 0)
		throw exc::SeqException("cannot collect elements of type '" + getInType()->getName() + "'");

	LLVMContext& context = module->getContext();
	BasicBlock *initBlock = getEnclosingInitBlock();

	auto *type = dynamic_cast<types::ArrayType *>(getOutType());
	assert(type != nullptr);

	if (!appendFunc) {
		appendFunc = cast<Function>(
		               module->getOrInsertFunction(
		                 "append",
		                 Type::getVoidTy(context),
		                 PointerType::get(IntegerType::getInt8PtrTy(context), 0),
		                 IntegerType::getInt8PtrTy(context),
		                 seqIntLLVM(context),
		                 seqIntLLVM(context),
		                 PointerType::get(seqIntLLVM(context), 0)));
	}

	block = prev->getAfter();
	BasicBlock *preambleBlock = getBase()->getPreamble();
	IRBuilder<> builder(block);

	Value *ptr = type->getBaseType()->alloc(getBase(), INIT_VEC_SIZE, preambleBlock);
	Value *ptrVar = makeAlloca(ptr, preambleBlock);
	Value *lenVar = makeAlloca(zeroLLVM(context), preambleBlock);
	Value *capVar = makeAlloca(ConstantInt::get(seqIntLLVM(context), INIT_VEC_SIZE), preambleBlock);
	Value *elemVar = makeAlloca(getInType()->getLLVMType(context), preambleBlock);

	Value *elemSize = ConstantInt::get(seqIntLLVM(context), (uint64_t)getInType()->size(module));
	Value *len = builder.CreateLoad(lenVar);
	Value *val = builder.CreateLoad(prev->result);

	type->getBaseType()->store(getBase(), val, elemVar, zeroLLVM(context), block);

	std::vector<Value *> args = {builder.CreatePointerCast(ptrVar,
	                                                       PointerType::get(IntegerType::getInt8PtrTy(context), 0)),
	                             builder.CreatePointerCast(elemVar,
	                                                       IntegerType::getInt8PtrTy(context)),
	                             elemSize,
	                             len,
	                             capVar};
	builder.CreateCall(appendFunc, args);

	Value *newLen = builder.CreateAdd(len, oneLLVM(context));
	builder.CreateStore(newLen, lenVar);

	Value *newPtr = builder.CreateLoad(ptrVar);
	Value *arr = type->make(newPtr, newLen, block);
	result = getOutType()->storeInAlloca(getBase(), arr, block, true);

	builder.SetInsertPoint(initBlock);
	Value *resultRead = getOutType()->loadFromAlloca(getBase(), result, initBlock);
	resultRead = getOutType()->setMemb(resultRead, "len", zeroLLVM(context), initBlock);
	getOutType()->store(getBase(), resultRead, result, zeroLLVM(context), initBlock);

	codegenNext(module);
	prev->setAfter(getAfter());
}

void Collect::finalize(Module *module, ExecutionEngine *eng)
{
	eng->addGlobalMapping(appendFunc, (void *)util::append);
	Stage::finalize(module, eng);
}

Collect& Collect::make()
{
	return *new Collect();
}
