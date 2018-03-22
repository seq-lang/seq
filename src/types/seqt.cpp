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

Type *types::SeqType::getLLVMType(LLVMContext& context)
{
	return llvm::Type::getInt8PtrTy(context);
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

seq_int_t types::SeqType::size() const
{
	return sizeof(char *);
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

seq_int_t types::MerType::size() const
{
	return k * sizeof(char);
}

types::MerType *types::MerType::get(seq_int_t k)
{
	return new MerType(k);
}
