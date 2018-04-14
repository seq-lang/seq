#include <iostream>
#include "seq/seq.h"
#include "seq/base.h"
#include "seq/seqt.h"

using namespace seq;
using namespace llvm;

SEQ_FUNC char *copySeq(char *seq, const seq_int_t len)
{
	auto *seq2 = (char *)std::malloc((size_t)len);
	std::memcpy(seq2, seq, (size_t)len);
	return seq2;
}

SEQ_FUNC void printSeq(char *seq, const seq_int_t len)
{
	for (seq_int_t i = 0; i < len; i++)
		std::cout << seq[i];
	std::cout << std::endl;
}

types::SeqType::SeqType() : Type("Seq", BaseType::get(), SeqData::SEQ)
{
	vtable.copy = (void *)copySeq;
	vtable.print = (void *)printSeq;
}

Function *types::SeqType::makeFuncOf(Module *module, Type *outType)
{
	static int idx = 1;
	LLVMContext& context = module->getContext();

	return cast<Function>(
	         module->getOrInsertFunction(
	           getName() + "Func" + std::to_string(idx++),
	           outType->getLLVMType(context),
	           IntegerType::getInt8PtrTy(context),
	           seqIntLLVM(context)));
}

void types::SeqType::setFuncArgs(Function *func,
                                 ValMap outs,
                                 BasicBlock *block)
{
	auto args = func->arg_begin();
	Value *seq = args++;
	Value *len = args;
	Value *seqVar = makeAlloca(seq, block);
	Value *lenVar = makeAlloca(len, block);
	outs->insert({SeqData::SEQ, seqVar});
	outs->insert({SeqData::LEN, lenVar});
}

Value *types::SeqType::callFuncOf(Function *func,
		                          ValMap outs,
                                  BasicBlock *block)
{
	IRBuilder<> builder(block);
	Value *seq = builder.CreateLoad(getSafe(outs, SeqData::SEQ));
	Value *len = builder.CreateLoad(getSafe(outs, SeqData::LEN));
	std::vector<Value *> args = {seq, len};
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

static inline std::string eqFuncName()
{
	return types::SeqType::get()->getName() + "Eq";
}

static Function *buildSeqEqFunc(Module *module)
{
	LLVMContext& context = module->getContext();

	Function *eq = cast<Function>(
	                 module->getOrInsertFunction(
	                   eqFuncName(),
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

Value *types::SeqType::checkEq(BaseFunc *base,
                               ValMap ins1,
                               ValMap ins2,
                               BasicBlock *block)
{
	Module *module = block->getModule();
	IRBuilder<> builder(block);
	Value *seq1 = builder.CreateLoad(getSafe(ins1, SeqData::SEQ));
	Value *len1 = builder.CreateLoad(getSafe(ins1, SeqData::LEN));
	Value *seq2 = builder.CreateLoad(getSafe(ins2, SeqData::SEQ));
	Value *len2 = builder.CreateLoad(getSafe(ins2, SeqData::LEN));

	Function *eq = module->getFunction(eqFuncName());

	if (!eq)
		eq = buildSeqEqFunc(module);

	std::vector<Value *> args = {seq1, len1, seq2, len2};
	return builder.CreateCall(eq, args);
}

void types::SeqType::callCopy(BaseFunc *base,
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
	                         seqIntLLVM(context)));

	copyFunc->setCallingConv(CallingConv::C);

	IRBuilder<> builder(block);
	Value *seq = builder.CreateLoad(getSafe(ins, SeqData::SEQ));
	Value *len = builder.CreateLoad(getSafe(ins, SeqData::LEN));
	std::vector<Value *> args = {seq, len};
	Value *copy = builder.CreateCall(copyFunc, args, "");

	Value *seqVar = makeAlloca(nullPtrLLVM(context), preambleBlock);
	Value *lenVar = makeAlloca(zeroLLVM(context), preambleBlock);

	builder.CreateStore(copy, seqVar);
	builder.CreateStore(len, lenVar);

	outs->insert({SeqData::SEQ, seqVar});
	outs->insert({SeqData::LEN, lenVar});
}

void types::SeqType::callPrint(BaseFunc *base,
                               ValMap outs,
                               BasicBlock *block)
{
	if (vtable.print == nullptr)
		throw exc::SeqException("cannot print specified type");

	LLVMContext& context = block->getContext();

	Function *printFunc = cast<Function>(
	                        block->getModule()->getOrInsertFunction(
	                          printFuncName(),
	                          llvm::Type::getVoidTy(context),
	                          IntegerType::getInt8PtrTy(context),
	                          seqIntLLVM(context)));

	printFunc->setCallingConv(CallingConv::C);

	IRBuilder<> builder(block);
	Value *seq = builder.CreateLoad(getSafe(outs, SeqData::SEQ));
	Value *len = builder.CreateLoad(getSafe(outs, SeqData::LEN));
	std::vector<Value *> args = {seq, len};
	builder.CreateCall(printFunc, args, "");
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

seq_int_t types::SeqType::size(Module *module) const
{
	std::unique_ptr<DataLayout> layout(new DataLayout(module));
	return layout->getTypeAllocSize(getLLVMType(module->getContext()));
}

types::SeqType *types::SeqType::get()
{
	static types::SeqType instance;
	return &instance;
}

Type *types::SeqType::getLLVMType(LLVMContext& context) const
{
	StructType *seqStruct = StructType::create(context, "seq_t");
	seqStruct->setBody({seqIntLLVM(context), IntegerType::getInt8PtrTy(context)});
	return seqStruct;
}
