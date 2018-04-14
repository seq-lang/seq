#include "seq/seq.h"
#include "seq/any.h"
#include "seq/base.h"
#include "seq/exc.h"
#include "seq/array.h"

using namespace seq;
using namespace llvm;

SEQ_FUNC void *copyArray(void *arr, seq_int_t len, seq_int_t elem_size)
{
	const size_t size = (size_t)len * elem_size;
	auto *arr2 = std::malloc(size);
	std::memcpy(arr2, arr, size);
	return arr2;
}

types::ArrayType::ArrayType(Type *baseType) :
    Type("Array", BaseType::get(), SeqData::ARRAY), baseType(baseType)
{
	vtable.copy = (void *)copyArray;
}

Function *types::ArrayType::makeFuncOf(Module *module, Type *outType)
{
	static int idx = 1;
	LLVMContext& context = module->getContext();

	return cast<Function>(
	         module->getOrInsertFunction(
	           getName() + "Func" + std::to_string(idx++),
	           outType->getLLVMType(context),
	           PointerType::get(getBaseType()->getLLVMType(context), 0),
	           seqIntLLVM(context)));
}

void types::ArrayType::setFuncArgs(Function *func,
                                   ValMap outs,
                                   BasicBlock *block)
{
	auto args = func->arg_begin();
	Value *ptr = args++;
	Value *len = args;
	Value *ptrVar = makeAlloca(ptr, block);
	Value *lenVar = makeAlloca(len, block);
	outs->insert({SeqData::ARRAY, ptrVar});
	outs->insert({SeqData::LEN, lenVar});
}

Value *types::ArrayType::callFuncOf(Function *func,
		                            ValMap outs,
                                    BasicBlock *block)
{
	IRBuilder<> builder(block);
	Value *ptr = builder.CreateLoad(getSafe(outs, SeqData::ARRAY));
	Value *len = builder.CreateLoad(getSafe(outs, SeqData::LEN));
	std::vector<Value *> args = {ptr, len};
	return builder.CreateCall(func, args);
}

Value *types::ArrayType::pack(BaseFunc *base,
                              ValMap outs,
                              BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	IRBuilder<> builder(block);

	Value *ptr = builder.CreateLoad(getSafe(outs, SeqData::ARRAY));
	Value *len = builder.CreateLoad(getSafe(outs, SeqData::LEN));

	Value *packed = builder.CreateInsertValue(UndefValue::get(getLLVMType(context)), len, {0});
	return builder.CreateInsertValue(packed, ptr, {1});
}

void types::ArrayType::unpack(BaseFunc *base,
                              Value *value,
                              ValMap outs,
                              BasicBlock *block)
{
	LLVMContext& context = base->getContext();
	BasicBlock *preambleBlock = base->getPreamble();
	IRBuilder<> builder(block);

	Value *ptr = builder.CreateExtractValue(value, {1});
	Value *len = builder.CreateExtractValue(value, {0});

	Value *ptrVar = makeAlloca(PointerType::get(getBaseType()->getLLVMType(context), 0), preambleBlock);
	Value *lenVar = makeAlloca(zeroLLVM(context), preambleBlock);

	builder.CreateStore(ptr, ptrVar);
	builder.CreateStore(len, lenVar);

	outs->insert({SeqData::ARRAY, ptrVar});
	outs->insert({SeqData::LEN, lenVar});
}

void types::ArrayType::callCopy(BaseFunc *base,
                                ValMap ins,
                                ValMap outs,
                                BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	BasicBlock *preambleBlock = base->getPreamble();

	Function *copyFunc = cast<Function>(
	                       block->getModule()->getOrInsertFunction(
	                         copyFuncName(),
	                         IntegerType::getInt8PtrTy(context),
	                         IntegerType::getInt8PtrTy(context),
	                         seqIntLLVM(context),
	                         seqIntLLVM(context)));

	copyFunc->setCallingConv(CallingConv::C);

	IRBuilder<> builder(block);
	Value *ptr = builder.CreateLoad(getSafe(ins, SeqData::ARRAY));
	Value *len = builder.CreateLoad(getSafe(ins, SeqData::LEN));
	Value *elemSize = ConstantInt::get(seqIntLLVM(context), (uint64_t)getBaseType()->size(block->getModule()));
	std::vector<Value *> args = {ptr, len, elemSize};
	Value *copy = builder.CreateCall(copyFunc, args, "");

	Value *ptrVar = makeAlloca(PointerType::get(getBaseType()->getLLVMType(context), 0), preambleBlock);
	Value *lenVar = makeAlloca(zeroLLVM(context), preambleBlock);

	builder.CreateStore(copy, ptrVar);
	builder.CreateStore(len, lenVar);

	outs->insert({SeqData::ARRAY, ptrVar});
	outs->insert({SeqData::LEN, lenVar});
}

void types::ArrayType::callSerialize(BaseFunc *base,
                                     ValMap outs,
                                     BasicBlock *block,
                                     std::string file)
{
	baseType->callSerializeArray(base, outs, block, file);
}

void types::ArrayType::finalizeSerialize(Module *module, ExecutionEngine *eng)
{
	baseType->finalizeSerializeArray(module, eng);
}

void types::ArrayType::callDeserialize(BaseFunc *base,
                                       ValMap outs,
                                       BasicBlock *block,
                                       std::string file)
{
	baseType->callDeserializeArray(base, outs, block, file);
}

void types::ArrayType::finalizeDeserialize(Module *module, ExecutionEngine *eng)
{
	baseType->finalizeDeserializeArray(module, eng);
}

void types::ArrayType::codegenLoad(BaseFunc *base,
                                   ValMap outs,
                                   BasicBlock *block,
                                   Value *ptr,
                                   Value *idx)
{
	LLVMContext& context = base->getContext();
	BasicBlock *preambleBlock = base->getPreamble();
	IRBuilder<> builder(block);

	Value *zero = ConstantInt::get(IntegerType::getInt32Ty(context), 0);
	Value *one  = ConstantInt::get(IntegerType::getInt32Ty(context), 1);

	Value *memPtr = builder.CreateGEP(ptr, {idx, one});
	Value *lenPtr = builder.CreateGEP(ptr, {idx, zero});

	Value *mem = builder.CreateLoad(memPtr);
	Value *len = builder.CreateLoad(lenPtr);

	Value *memVar = makeAlloca(PointerType::get(getBaseType()->getLLVMType(context), 0), preambleBlock);
	Value *lenVar = makeAlloca(zeroLLVM(context), preambleBlock);

	builder.CreateStore(mem, memVar);
	builder.CreateStore(len, lenVar);

	outs->insert({SeqData::ARRAY, memVar});
	outs->insert({SeqData::LEN, lenVar});
}

void types::ArrayType::codegenStore(BaseFunc *base,
                                    ValMap outs,
                                    BasicBlock *block,
                                    Value *ptr,
                                    Value *idx)
{
	LLVMContext& context = base->getContext();
	IRBuilder<> builder(block);

	Value *arr = builder.CreateLoad(getSafe(outs, SeqData::ARRAY));
	Value *len = builder.CreateLoad(getSafe(outs, SeqData::LEN));

	Value *zero = ConstantInt::get(IntegerType::getInt32Ty(context), 0);
	Value *one  = ConstantInt::get(IntegerType::getInt32Ty(context), 1);

	Value *arrPtr = builder.CreateGEP(ptr, {idx, one});
	Value *lenPtr = builder.CreateGEP(ptr, {idx, zero});

	builder.CreateStore(arr, arrPtr);
	builder.CreateStore(len, lenPtr);
}

void types::ArrayType::codegenIndexLoad(BaseFunc *base,
                                        ValMap outs,
                                        BasicBlock *block,
                                        Value *ptr,
                                        Value *idx)
{
	getBaseType()->codegenLoad(base, outs, block, ptr, idx);
}

void types::ArrayType::codegenIndexStore(BaseFunc *base,
                                         ValMap outs,
                                         BasicBlock *block,
                                         Value *ptr,
                                         Value *idx)
{
	getBaseType()->codegenStore(base, outs, block, ptr, idx);
}

bool types::ArrayType::isChildOf(Type *type) const
{
	if (type == BaseType::get())
		return true;

	auto *arrayType = dynamic_cast<types::ArrayType *>(type);
	return arrayType && getBaseType()->is(arrayType->getBaseType());
}

types::Type *types::ArrayType::getBaseType() const
{
	return getBaseType(0);
}

types::Type *types::ArrayType::getBaseType(seq_int_t idx) const
{
	return baseType;
}

Type *types::ArrayType::getLLVMType(LLVMContext& context) const
{
	llvm::StructType *arrStruct = StructType::create(context, "arr_t");
	arrStruct->setBody({seqIntLLVM(context),
	                    PointerType::get(baseType->getLLVMType(context), 0)});
	return arrStruct;
}

seq_int_t types::ArrayType::size(Module *module) const
{
	std::unique_ptr<DataLayout> layout(new DataLayout(module));
	return layout->getTypeAllocSize(getLLVMType(module->getContext()));
}

types::ArrayType& types::ArrayType::of(Type& baseType) const
{
	return *ArrayType::get(&baseType);
}

types::ArrayType *types::ArrayType::get(Type *baseType)
{
	return new ArrayType(baseType);
}

types::ArrayType *types::ArrayType::get()
{
	return new ArrayType(types::BaseType::get());
}
