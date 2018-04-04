#include "seq/seq.h"
#include "seq/util.h"
#include "seq/collect.h"

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
	BasicBlock *preambleBlock = getBase()->getPreamble();
	IRBuilder<> builder(block);

	Value *ptr = type->getBaseType()->codegenAlloc(getBase(), INIT_VEC_SIZE, preambleBlock);
	Value *ptrVar = makeAlloca(ptr, preambleBlock);
	Value *lenVar = makeAlloca(zeroLLVM(context), preambleBlock);
	Value *capVar = makeAlloca(ConstantInt::get(seqIntLLVM(context), INIT_VEC_SIZE), preambleBlock);
	Value *elemVar = makeAlloca(getInType()->getLLVMType(context), preambleBlock);

	Value *elemSize = ConstantInt::get(seqIntLLVM(context), (uint64_t)getInType()->size());
	Value *len = builder.CreateLoad(lenVar);

	type->getBaseType()->codegenStore(getBase(),
	                                  prev->outs,
	                                  block,
	                                  elemVar,
	                                  zeroLLVM(context));

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

	outs->insert({SeqData::ARRAY, ptrVar});
	outs->insert({SeqData::LEN, lenVar});

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
