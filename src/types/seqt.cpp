#include <iostream>
#include "seq.h"
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

Function *types::SeqType::makeFuncOf(Module *module,
                                     ValMap outs,
                                     Type *outType)
{
	static int idx = 1;
	LLVMContext& context = module->getContext();

	Function *func = cast<Function>(
	                   module->getOrInsertFunction(
	                     getName() + "Func" + std::to_string(idx++),
	                     outType->getLLVMType(context),
	                     PointerType::get(IntegerType::getInt8PtrTy(context), 0),
	                     PointerType::get(seqIntLLVM(context), 0)));

	auto args = func->arg_begin();
	Value *seqVar = args++;
	Value *lenVar = args;
	outs->insert({SeqData::SEQ, seqVar});
	outs->insert({SeqData::LEN, lenVar});
	return func;
}

Value *types::SeqType::callFuncOf(llvm::Function *func,
		                          ValMap outs,
                                  llvm::BasicBlock *block)
{
	IRBuilder<> builder(block);
	Value *seqVar = getSafe(outs, SeqData::SEQ);
	Value *lenVar = getSafe(outs, SeqData::LEN);
	std::vector<Value *> args = {seqVar, lenVar};
	return builder.CreateCall(func, args);
}

Value *types::SeqType::pack(BaseFunc *base,
                            ValMap outs,
                            BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	IRBuilder<> builder(block);

	Value *seq = builder.CreateLoad(getSafe(outs, SeqData::SEQ));
	Value *len = builder.CreateLoad(getSafe(outs, SeqData::LEN));

	Value *packed = builder.CreateInsertValue(UndefValue::get(getLLVMType(context)), len, {0});
	return builder.CreateInsertValue(packed, seq, {1});
}

void types::SeqType::unpack(BaseFunc *base,
                            Value *value,
                            ValMap outs,
                            BasicBlock *block)
{
	LLVMContext& context = base->getContext();
	BasicBlock *preambleBlock = base->getPreamble();
	IRBuilder<> builder(block);

	Value *seq = builder.CreateExtractValue(value, {1});
	Value *len = builder.CreateExtractValue(value, {0});

	Value *seqVar = makeAlloca(nullPtrLLVM(context), preambleBlock);
	Value *lenVar = makeAlloca(zeroLLVM(context), preambleBlock);

	builder.CreateStore(seq, seqVar);
	builder.CreateStore(len, lenVar);

	outs->insert({SeqData::SEQ, seqVar});
	outs->insert({SeqData::LEN, lenVar});
}

static Function *buildSeqEqFunc(Module *module)
{
	LLVMContext& context = module->getContext();

	Function *eq = cast<Function>(
	                 module->getOrInsertFunction(
                       "seq_eq",
	                   IntegerType::getInt1Ty(context),
	                   IntegerType::getInt8PtrTy(context),
                       seqIntLLVM(context),
                       IntegerType::getInt8PtrTy(context),
	                   seqIntLLVM(context)));

	auto args = eq->arg_begin();
	Value *seq1 = args++;
	Value *len1 = args++;
	Value *seq2 = args++;
	Value *len2 = args;
	seq1->setName("seq1");
	len1->setName("len1");
	seq2->setName("seq2");
	len2->setName("len2");

	BasicBlock *entry         = BasicBlock::Create(context, "entry", eq);
	BasicBlock *exitEarly     = BasicBlock::Create(context, "exit_early", eq);
	BasicBlock *compareSeqs   = BasicBlock::Create(context, "compare_seqs", eq);
	BasicBlock *breakBlock    = BasicBlock::Create(context, "break", eq);
	BasicBlock *continueBlock = BasicBlock::Create(context, "continue", eq);
	BasicBlock *exit          = BasicBlock::Create(context, "exit", eq);

	IRBuilder<> builder(entry);

	/* entry */
	Value *lenEq = builder.CreateICmpEQ(len1, len2);
	builder.CreateCondBr(lenEq, compareSeqs, exitEarly);

	/* exit early (different lengths) */
	builder.SetInsertPoint(exitEarly);
	builder.CreateRet(ConstantInt::get(IntegerType::getInt1Ty(context), 0));

	/* sequence comparison loop */
	builder.SetInsertPoint(compareSeqs);
	PHINode *control = builder.CreatePHI(seqIntLLVM(context), 2, "i");
	control->addIncoming(ConstantInt::get(seqIntLLVM(context), 0), entry);

	Value *seqPtr1 = builder.CreateGEP(seq1, control);
	Value *seqPtr2 = builder.CreateGEP(seq2, control);
	Value *char1 = builder.CreateLoad(seqPtr1);
	Value *char2 = builder.CreateLoad(seqPtr2);
	Value *charEq = builder.CreateICmpEQ(char1, char2);
	builder.CreateCondBr(charEq, continueBlock, breakBlock);

	/* break (unequal characters) */
	builder.SetInsertPoint(breakBlock);
	builder.CreateRet(ConstantInt::get(IntegerType::getInt1Ty(context), 0));

	/* continue (equal characters) */
	builder.SetInsertPoint(continueBlock);
	Value *next = builder.CreateAdd(control,
	                                ConstantInt::get(seqIntLLVM(context), 1),
	                                "next");
	Value *cond = builder.CreateICmpSLT(next, len1);
	builder.CreateCondBr(cond, compareSeqs, exit);
	control->addIncoming(next, continueBlock);

	/* exit (loop finished; return 1) */
	builder.SetInsertPoint(exit);
	builder.CreateRet(ConstantInt::get(IntegerType::getInt1Ty(context), 1));

	return eq;
}

void types::SeqType::checkEq(BaseFunc *base,
                             ValMap ins1,
                             ValMap ins2,
                             ValMap outs,
                             llvm::BasicBlock *block)
{
	LLVMContext& context = base->getContext();
	BasicBlock *preambleBlock = base->getPreamble();
	IRBuilder<> builder(block);

	Value *seq1 = builder.CreateLoad(getSafe(ins1, SeqData::SEQ));
	Value *len1 = builder.CreateLoad(getSafe(ins1, SeqData::LEN));

	Value *seq2 = builder.CreateLoad(getSafe(ins1, SeqData::SEQ));
	Value *len2 = builder.CreateLoad(getSafe(ins1, SeqData::LEN));

	static Function *eq = nullptr;

	if (!eq)
		eq = buildSeqEqFunc(block->getModule());

	std::vector<Value *> args = {seq1, len1, seq2, len2};
	Value *result = builder.CreateCall(eq, args);
	Value *resultVar = makeAlloca(ConstantInt::get(IntegerType::getInt1Ty(context), 0), preambleBlock);
	builder.CreateStore(result, resultVar);

	outs->insert({SeqData::BOOL, resultVar});
}

void types::SeqType::callPrint(BaseFunc *base,
                               ValMap outs,
                               BasicBlock *block)
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

	IRBuilder<> builder(block);
	Value *seq = builder.CreateLoad(getSafe(outs, SeqData::SEQ));
	Value *len = builder.CreateLoad(getSafe(outs, SeqData::LEN));
	std::vector<Value *> args = {seq, len};
	builder.CreateCall(vtable.printFunc, args, "");
}

void types::SeqType::codegenLoad(BaseFunc *base,
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

	Value *seqPtr = builder.CreateGEP(ptr, {idx, one});
	Value *lenPtr = builder.CreateGEP(ptr, {idx, zero});

	Value *seq = builder.CreateLoad(seqPtr);
	Value *len = builder.CreateLoad(lenPtr);

	Value *seqVar = makeAlloca(nullPtrLLVM(context), preambleBlock);
	Value *lenVar = makeAlloca(zeroLLVM(context), preambleBlock);

	builder.CreateStore(seq, seqVar);
	builder.CreateStore(len, lenVar);

	outs->insert({SeqData::SEQ, seqVar});
	outs->insert({SeqData::LEN, lenVar});
}

void types::SeqType::codegenStore(BaseFunc *base,
                                  ValMap outs,
                                  BasicBlock *block,
                                  Value *ptr,
                                  Value *idx)
{
	LLVMContext& context = block->getContext();
	IRBuilder<> builder(block);

	Value *seq = builder.CreateLoad(getSafe(outs, SeqData::SEQ));
	Value *len = builder.CreateLoad(getSafe(outs, SeqData::LEN));

	Value *zero = ConstantInt::get(IntegerType::getInt32Ty(context), 0);
	Value *one  = ConstantInt::get(IntegerType::getInt32Ty(context), 1);

	Value *seqPtr = builder.CreateGEP(ptr, {idx, one});
	Value *lenPtr = builder.CreateGEP(ptr, {idx, zero});

	builder.CreateStore(seq, seqPtr);
	builder.CreateStore(len, lenPtr);
}

seq_int_t types::SeqType::size() const
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
