#include <any.h>
#include "base.h"
#include "exc.h"
#include "array.h"

using namespace seq;
using namespace llvm;

types::ArrayType::ArrayType(Type *base) :
    Type("Array", BaseType::get(), SeqData::ARRAY), base(base), arrStruct(nullptr)
{
}

void types::ArrayType::callSerialize(ValMap outs,
                                     BasicBlock *block,
                                     std::string file)
{
	base->callSerializeArray(outs, block, file);
}

void types::ArrayType::finalizeSerialize(ExecutionEngine *eng)
{
	base->finalizeSerializeArray(eng);
}

void types::ArrayType::callDeserialize(ValMap outs,
                                       BasicBlock *block,
                                       std::string file)
{
	base->callDeserializeArray(outs, block, file);
}

void types::ArrayType::finalizeDeserialize(ExecutionEngine *eng)
{
	base->finalizeDeserializeArray(eng);
}

void types::ArrayType::codegenLoad(ValMap outs,
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

void types::ArrayType::codegenStore(ValMap outs,
                                    BasicBlock *block,
                                    Value *ptr,
                                    Value *idx)
{
	auto arriter = outs->find(SeqData::ARRAY);
	auto leniter = outs->find(SeqData::LEN);

	if (arriter == outs->end() || leniter == outs->end())
		throw exc::SeqException("pipeline error");

	LLVMContext& context = block->getContext();
	Value *arr = arriter->second;
	Value *len = leniter->second;

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
	return PointerType::get(base->getLLVMArrayType(context), 0);
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
	return base;
}

types::ArrayType& types::ArrayType::of(Type& base) const
{
	return *ArrayType::get(&base);
}

types::ArrayType *types::ArrayType::get(Type *base)
{
	return new types::ArrayType(base);
}

types::ArrayType *types::ArrayType::get()
{
	return new types::ArrayType(types::BaseType::get());
}
