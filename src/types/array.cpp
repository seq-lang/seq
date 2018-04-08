#include "seq/seq.h"
#include "seq/any.h"
#include "seq/base.h"
#include "seq/exc.h"
#include "seq/array.h"

using namespace seq;
using namespace llvm;

types::ArrayType::ArrayType(Type *baseType) :
    Type("Array", BaseType::get(), SeqData::ARRAY), baseType(baseType)
{
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

bool types::ArrayType::isChildOf(Type *type)
{
	if (type == BaseType::get())
		return true;

	auto *arrayType = dynamic_cast<types::ArrayType *>(type);
	return arrayType && getBaseType()->isChildOf(arrayType->getBaseType());
}

Type *types::ArrayType::getLLVMType(LLVMContext& context)
{
	llvm::StructType *arrStruct = StructType::create(context, "arr_t");
	arrStruct->setBody({seqIntLLVM(context),
	                    PointerType::get(baseType->getLLVMType(context), 0)});
	return arrStruct;
}

seq_int_t types::ArrayType::size() const
{
	return sizeof(seq_int_t) + sizeof(void *);
}

types::Type *types::ArrayType::getBaseType() const
{
	return baseType;
}

types::ArrayType& types::ArrayType::of(Type& baseType) const
{
	return *ArrayType::get(&baseType);
}

types::ArrayType *types::ArrayType::get(Type *baseType)
{
	return new types::ArrayType(baseType);
}

types::ArrayType *types::ArrayType::get()
{
	return new types::ArrayType(types::BaseType::get());
}
