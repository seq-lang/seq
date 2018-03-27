#include <any.h>
#include "seq.h"
#include "base.h"
#include "exc.h"
#include "array.h"

using namespace seq;
using namespace llvm;

types::ArrayType::ArrayType(Type *baseType) :
    Type("Array", BaseType::get(), SeqData::ARRAY),
    baseType(baseType), arrStruct(nullptr)
{
}

void types::ArrayType::callSerialize(seq::Seq *base,
                                     ValMap outs,
                                     BasicBlock *block,
                                     std::string file)
{
	baseType->callSerializeArray(base, outs, block, file);
}

void types::ArrayType::finalizeSerialize(ExecutionEngine *eng)
{
	baseType->finalizeSerializeArray(eng);
}

void types::ArrayType::callDeserialize(seq::Seq *base,
                                       ValMap outs,
                                       BasicBlock *block,
                                       std::string file)
{
	baseType->callDeserializeArray(base, outs, block, file);
}

void types::ArrayType::finalizeDeserialize(ExecutionEngine *eng)
{
	baseType->finalizeDeserializeArray(eng);
}

void types::ArrayType::codegenLoad(seq::Seq *base,
                                   ValMap outs,
                                   BasicBlock *block,
                                   Value *ptr,
                                   Value *idx)
{
	LLVMContext& context = block->getContext();
	IRBuilder<> builder(block);

	Value *zero = ConstantInt::get(IntegerType::getInt32Ty(context), 0);
	Value *one  = ConstantInt::get(IntegerType::getInt32Ty(context), 1);

	Value *arrPtr = builder.CreateGEP(ptr, {idx, one});
	Value *lenPtr = builder.CreateGEP(ptr, {idx, zero});

	outs->insert({SeqData::ARRAY, arrPtr});
	outs->insert({SeqData::LEN,   builder.CreateLoad(lenPtr)});
}

void types::ArrayType::codegenStore(seq::Seq *base,
                                    ValMap outs,
                                    BasicBlock *block,
                                    Value *ptr,
                                    Value *idx)
{
	LLVMContext& context = block->getContext();
	Value *arr = getSafe(outs, SeqData::ARRAY);
	Value *len = getSafe(outs, SeqData::LEN);

	IRBuilder<> builder(block);

	Value *zero = ConstantInt::get(IntegerType::getInt32Ty(context), 0);
	Value *one  = ConstantInt::get(IntegerType::getInt32Ty(context), 1);

	Value *arrPtr = builder.CreateGEP(ptr, {idx, one});
	Value *lenPtr = builder.CreateGEP(ptr, {idx, zero});

	builder.CreateStore(builder.CreateLoad(arr), arrPtr);
	builder.CreateStore(len, lenPtr);
}

Type *types::ArrayType::getLLVMType(LLVMContext& context)
{
	return PointerType::get(baseType->getLLVMArrayType(context), 0);
}

Type *types::ArrayType::getLLVMArrayType(LLVMContext& context)
{
	if (!arrStruct) {
		arrStruct = StructType::create(context, "arr_t");
		arrStruct->setBody({seqIntLLVM(context), getLLVMType(context)});
	}

	return arrStruct;
}

seq_int_t types::ArrayType::size() const
{
	return sizeof(void *);
}

seq_int_t types::ArrayType::arraySize() const
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
