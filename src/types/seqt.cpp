#include <iostream>
#include "base.h"
#include "seqt.h"

using namespace seq;
using namespace llvm;

SEQ_FUNC void printSeq(char *seq, const seq_int_t len)
{
	for (seq_int_t i = 0; i < len; i++)
		std::cout << seq[i];
	std::cout << std::endl;
}

SEQ_FUNC void printMer(char *seq, const seq_int_t len)
{
	printSeq(seq, len);
}

types::SeqType::SeqType() : Type("Seq", BaseType::get(), SeqData::SEQ)
{
	vtable.print = (void *)printSeq;
}

void types::SeqType::callPrint(ValMap outs, BasicBlock *block)
{
	if (vtable.print == nullptr)
		throw exc::SeqException("cannot print specified type");

	LLVMContext& context = block->getContext();

	if (!vtable.printFunc) {
		vtable.printFunc = cast<Function>(
		                     block->getModule()->getOrInsertFunction(
		                       "print" + getName(),
		                       llvm::Type::getVoidTy(context),
		                       IntegerType::getInt8PtrTy(context),
		                       seqIntLLVM(context)));

		vtable.printFunc->setCallingConv(CallingConv::C);
	}

	auto seqiter = outs->find(SeqData::SEQ);
	auto leniter = outs->find(SeqData::LEN);

	if (seqiter == outs->end() || leniter == outs->end())
		throw exc::SeqException("pipeline error");

	IRBuilder<> builder(block);
	std::vector<Value *> args = {seqiter->second, leniter->second};
	builder.CreateCall(vtable.printFunc, args, "");
}

void types::SeqType::callAlloc(ValMap outs, seq_int_t count, BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	Module *module = block->getModule();

	if (!vtable.allocFunc) {
		vtable.allocFunc = cast<Function>(
		                     module->getOrInsertFunction(
		                       "malloc",
		                       IntegerType::getInt8PtrTy(context),
		                       IntegerType::getIntNTy(context, sizeof(size_t) * 8)));
	}

	IRBuilder<> builder(block);

	GlobalVariable *ptr = new GlobalVariable(*module,
	                                         PointerType::get(getLLVMArrayType(context), 0),
	                                         false,
	                                         GlobalValue::PrivateLinkage,
	                                         nullptr,
	                                         "mem");

	ptr->setInitializer(
	  ConstantPointerNull::get(PointerType::get(getLLVMArrayType(context), 0)));

	std::vector<Value *> args = {
	  ConstantInt::get(IntegerType::getIntNTy(context, sizeof(size_t)*8), (unsigned)(count * arraySize()))};
	Value *mem = builder.CreateCall(vtable.allocFunc, args);
	mem = builder.CreatePointerCast(mem, PointerType::get(getLLVMArrayType(context), 0));
	builder.CreateStore(mem, ptr);

	outs->insert({SeqData::ARRAY, ptr});
	outs->insert({SeqData::LEN, ConstantInt::get(seqIntLLVM(context), (uint64_t)count)});
}

void types::SeqType::codegenLoad(ValMap outs,
                                 BasicBlock *block,
                                 Value *ptr,
                                 Value *idx)
{
	LLVMContext& context = block->getContext();
	IRBuilder<> builder(block);

	Value *zero = ConstantInt::get(IntegerType::getInt32Ty(context), 0);
	Value *one  = ConstantInt::get(IntegerType::getInt32Ty(context), 1);

	Value *seqPtr = builder.CreateGEP(ptr, {idx, one});
	Value *lenPtr = builder.CreateGEP(ptr, {idx, zero});

	outs->insert({SeqData::SEQ, builder.CreateLoad(seqPtr)});
	outs->insert({SeqData::LEN, builder.CreateLoad(lenPtr)});
}

void types::SeqType::codegenStore(ValMap outs,
                                  BasicBlock *block,
                                  Value *ptr,
                                  Value *idx)
{
	auto seqiter = outs->find(SeqData::SEQ);
	auto leniter = outs->find(SeqData::LEN);

	if (seqiter == outs->end() || leniter == outs->end())
		throw exc::SeqException("pipeline error");

	LLVMContext& context = block->getContext();
	Value *seq = seqiter->second;
	Value *len = leniter->second;

	IRBuilder<> builder(block);

	Value *zero = ConstantInt::get(IntegerType::getInt32Ty(context), 0);
	Value *one  = ConstantInt::get(IntegerType::getInt32Ty(context), 1);

	Value *seqPtr = builder.CreateGEP(ptr, {idx, one});
	Value *lenPtr = builder.CreateGEP(ptr, {idx, zero});

	builder.CreateStore(seq, seqPtr);
	builder.CreateStore(len, lenPtr);
}

seq_int_t types::SeqType::size() const
{
	return sizeof(char *);
}

seq_int_t types::SeqType::arraySize() const
{
	return sizeof(seq_int_t) + sizeof(char *);
}

types::SeqType *types::SeqType::get()
{
	static types::SeqType instance;
	return &instance;
}

types::MerType::MerType(seq_int_t k) :
    Type("Mer", SeqType::get(), SeqData::SEQ), k(k)
{
	vtable.print = (void *)printMer;
}

Type *types::MerType::getLLVMType(llvm::LLVMContext& context)
{
	return IntegerType::getIntNTy(context, (unsigned)(2*k));
}

Type *types::SeqType::getLLVMType(LLVMContext& context)
{
	return llvm::Type::getInt8PtrTy(context);
}

Type *types::SeqType::getLLVMArrayType(LLVMContext& context)
{
	static StructType *seqStruct = nullptr;

	if (!seqStruct) {
		seqStruct = StructType::create(context, "seq_t");
		seqStruct->setBody({seqIntLLVM(context), IntegerType::getInt8PtrTy(context)});
	}

	return seqStruct;
}

seq_int_t types::MerType::size() const
{
	return k * sizeof(char);
}

types::MerType *types::MerType::get(seq_int_t k)
{
	return new MerType(k);
}
