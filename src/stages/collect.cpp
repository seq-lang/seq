#include "seq.h"
#include "util.h"
#include "collect.h"

using namespace seq;
using namespace llvm;

static const size_t INIT_VEC_SIZE = 10;

Collect::Collect() :
    Stage("collect", types::VoidType::get(), types::VoidType::get()), appendFunc(nullptr)
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

	if (getInType()->getKey() == SeqData::NONE || getInType()->size() == 0)
		throw exc::SeqException("cannot collect elements of type '" + getInType()->getName() + "'");

	LLVMContext& context = module->getContext();
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

	block = prev->block;
	IRBuilder<> builder(block);
	IRBuilder<> preamble(getBase()->getPreamble());

	type->getBaseType()->callAlloc(outs, INIT_VEC_SIZE, getBase()->getPreamble());

	auto arriter = outs->find(SeqData::ARRAY);
	auto leniter = outs->find(SeqData::LEN);
	assert(arriter != outs->end() && leniter != outs->end());

	Value *ptr = arriter->second;

	Value *len = preamble.CreateAlloca(seqIntLLVM(context),
	                                   ConstantInt::get(seqIntLLVM(context), 1));
	preamble.CreateStore(ConstantInt::get(seqIntLLVM(context), 0), len);

	Value *cap = preamble.CreateAlloca(seqIntLLVM(context),
	                                   ConstantInt::get(seqIntLLVM(context), 1));
	preamble.CreateStore(ConstantInt::get(seqIntLLVM(context), INIT_VEC_SIZE), cap);

	Value *elemPtr = preamble.CreateAlloca(getInType()->getLLVMArrayType(context),
	                                       ConstantInt::get(seqIntLLVM(context), 1));

	Value *elemSize = ConstantInt::get(seqIntLLVM(context), (uint64_t)getInType()->arraySize());
	Value *lenActual = builder.CreateLoad(len);

	type->getBaseType()->codegenStore(prev->outs, block, elemPtr, ConstantInt::get(seqIntLLVM(context), 0));

	std::vector<Value *> args = {builder.CreatePointerCast(ptr,
	                                                       PointerType::get(IntegerType::getInt8PtrTy(context), 0)),
	                             builder.CreatePointerCast(elemPtr,
	                                                       IntegerType::getInt8PtrTy(context)),
	                             elemSize,
	                             lenActual,
	                             cap};
	builder.CreateCall(appendFunc, args);

	Value *newLen = builder.CreateAdd(lenActual, ConstantInt::get(seqIntLLVM(context), 1));
	builder.CreateStore(newLen, len);

	leniter->second = newLen;

	codegenNext(module);
	prev->setAfter(getAfter());
}

void Collect::finalize(ExecutionEngine *eng)
{
	eng->addGlobalMapping(appendFunc, (void *)util::append);
	Stage::finalize(eng);
}

Collect& Collect::make()
{
	return *new Collect();
}
