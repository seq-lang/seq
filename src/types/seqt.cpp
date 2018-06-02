#include <iostream>
#include "seq/seq.h"
#include "seq/base.h"
#include "seq/seqt.h"

using namespace seq;
using namespace llvm;

SEQ_FUNC char *copyBaseSeq(char *seq, const seq_int_t len)
{
	auto *seq2 = (char *)std::malloc((size_t)len);
	std::memcpy(seq2, seq, (size_t)len);
	return seq2;
}

SEQ_FUNC void printBaseSeq(char *seq, const seq_int_t len)
{
	for (seq_int_t i = 0; i < len; i++)
		std::cout << seq[i];
	std::cout << std::endl;
}

types::BaseSeqType::BaseSeqType(std::string name, SeqData key) :
    Type(std::move(name), BaseType::get(), key)
{
	vtable.copy = (void *)copyBaseSeq;
	vtable.print = (void *)printBaseSeq;
}

static inline std::string eqFuncName()
{
	return "BaseSeqEq";
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

Value *types::BaseSeqType::eq(BaseFunc *base,
                              Value *self,
                              Value *other,
                              BasicBlock *block)
{
	Module *module = block->getModule();
	Value *seq1 = Seq.memb(self, "ptr", block);
	Value *len1 = Seq.memb(self, "len", block);
	Value *seq2 = Seq.memb(other, "ptr", block);
	Value *len2 = Seq.memb(other, "len", block);

	Function *eq = module->getFunction(eqFuncName());

	if (!eq)
		eq = buildSeqEqFunc(module);

	IRBuilder<> builder(block);
	return builder.CreateCall(eq, {seq1, len1, seq2, len2});
}

Value *types::BaseSeqType::copy(BaseFunc *base,
                                Value *self,
                                BasicBlock *block)
{
	LLVMContext& context = block->getContext();

	Function *copyFunc = cast<Function>(
	                       block->getModule()->getOrInsertFunction(
	                         copyFuncName(),
	                         IntegerType::getInt8PtrTy(context),
	                         IntegerType::getInt8PtrTy(context),
	                         seqIntLLVM(context)));

	copyFunc->setCallingConv(CallingConv::C);

	Value *seq = Seq.memb(self, "ptr", block);
	Value *len = Seq.memb(self, "len", block);

	IRBuilder<> builder(block);
	Value *copy = builder.CreateCall(copyFunc, {seq, len});
	return make(copy, len, block);
}

void types::BaseSeqType::serialize(BaseFunc *base,
                                   Value *self,
                                   Value *fp,
                                   BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	Module *module = block->getModule();

	Function *writeFunc = cast<Function>(
	                        module->getOrInsertFunction(
	                          IO_WRITE_FUNC_NAME,
	                          llvm::Type::getVoidTy(context),
	                          IntegerType::getInt8PtrTy(context),
	                          seqIntLLVM(context),
	                          seqIntLLVM(context),
	                          IntegerType::getInt8PtrTy(context)));

	writeFunc->setCallingConv(CallingConv::C);

	Value *seq = Seq.memb(self, "ptr", block);
	Value *len = Seq.memb(self, "len", block);
	Int.serialize(base, len, fp, block);

	IRBuilder<> builder(block);
	builder.CreateCall(writeFunc, {seq, len, oneLLVM(context), fp});
}

Value *types::BaseSeqType::deserialize(BaseFunc *base,
                                       Value *fp,
                                       BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	Module *module = block->getModule();

	Function *readFunc = cast<Function>(
	                       module->getOrInsertFunction(
	                         IO_READ_FUNC_NAME,
	                         llvm::Type::getVoidTy(context),
	                         IntegerType::getInt8PtrTy(context),
	                         seqIntLLVM(context),
	                         seqIntLLVM(context),
	                         IntegerType::getInt8PtrTy(context)));

	Function *allocFunc = cast<Function>(
	                        module->getOrInsertFunction(
	                          allocFuncName(),
	                          IntegerType::getInt8PtrTy(context),
	                          IntegerType::getIntNTy(context, sizeof(size_t)*8)));

	readFunc->setCallingConv(CallingConv::C);

	IRBuilder<> builder(block);

	Value *len = Int.deserialize(base, fp, block);
	Value *seq = builder.CreateCall(allocFunc, {len});
	builder.CreateCall(readFunc, {seq, len, oneLLVM(context), fp});
	return make(seq, len, block);
}

void types::BaseSeqType::print(BaseFunc *base,
                               Value *self,
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

	Value *seq = Seq.memb(self, "ptr", block);
	Value *len = Seq.memb(self, "len", block);

	IRBuilder<> builder(block);
	builder.CreateCall(printFunc, {seq, len});
}

Value *types::BaseSeqType::defaultValue(BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	Value *ptr = ConstantPointerNull::get(IntegerType::getInt8PtrTy(context));
	Value *len = zeroLLVM(context);
	return make(ptr, len, block);
}

void types::BaseSeqType::initFields()
{
	if (!vtable.fields.empty())
		return;

	vtable.fields = {
		{"len", {0, &Int}},
		{"ptr", {1, &Void}}
	};
}

seq_int_t types::BaseSeqType::size(Module *module) const
{
	std::unique_ptr<DataLayout> layout(new DataLayout(module));
	return layout->getTypeAllocSize(getLLVMType(module->getContext()));
}

/* derived Seq type */
types::SeqType::SeqType() : BaseSeqType("Seq", SeqData::SEQ)
{
}

Value *types::SeqType::memb(Value *self,
                            const std::string& name,
                            BasicBlock *block)
{
	return BaseSeqType::memb(self, name, block);
}

Value *types::SeqType::setMemb(Value *self,
                               const std::string& name,
                               Value *val,
                               BasicBlock *block)
{
	return BaseSeqType::setMemb(self, name, val, block);
}

Type *types::SeqType::getLLVMType(LLVMContext& context) const
{
	StructType *seqStruct = StructType::create(context, "seq_t");
	seqStruct->setBody({seqIntLLVM(context), IntegerType::getInt8PtrTy(context)});
	return seqStruct;
}

Value *types::SeqType::make(Value *ptr, Value *len, BasicBlock *block)
{
	LLVMContext& context = ptr->getContext();
	Value *self = UndefValue::get(getLLVMType(context));
	self = Seq.setMemb(self, "ptr", ptr, block);
	self = Seq.setMemb(self, "len", len, block);
	return self;
}

types::SeqType *types::SeqType::get()
{
	static types::SeqType instance;
	return &instance;
}

/* derived Str type */
types::StrType::StrType() : BaseSeqType("Str", SeqData::STR)
{
}

Value *types::StrType::memb(Value *self,
                            const std::string& name,
                            BasicBlock *block)
{
	return BaseSeqType::memb(self, name, block);
}

Value *types::StrType::setMemb(Value *self,
                               const std::string& name,
                               Value *val,
                               BasicBlock *block)
{
	return BaseSeqType::setMemb(self, name, val, block);
}

Type *types::StrType::getLLVMType(LLVMContext& context) const
{
	StructType *seqStruct = StructType::create(context, "str_t");
	seqStruct->setBody({seqIntLLVM(context), IntegerType::getInt8PtrTy(context)});
	return seqStruct;
}

Value *types::StrType::make(Value *ptr, Value *len, BasicBlock *block)
{
	LLVMContext& context = ptr->getContext();
	Value *self = UndefValue::get(getLLVMType(context));
	self = Str.setMemb(self, "ptr", ptr, block);
	self = Str.setMemb(self, "len", len, block);
	return self;
}

types::StrType *types::StrType::get()
{
	static types::StrType instance;
	return &instance;
}
