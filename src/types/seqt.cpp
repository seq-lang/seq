#include <iostream>
#include "seq/seq.h"

using namespace seq;
using namespace llvm;

types::BaseSeqType::BaseSeqType(std::string name) :
    Type(std::move(name), BaseType::get())
{
	SEQ_ASSIGN_VTABLE_FIELD(copy, seq_copy_seq);
	SEQ_ASSIGN_VTABLE_FIELD(print, seq_print_seq);
}

static inline std::string eqFuncName()
{
	return "seq.baseseq.eq";
}

static Function *buildSeqEqFunc(Module *module)
{
	LLVMContext& context = module->getContext();

	auto *eq = cast<Function>(
	             module->getOrInsertFunction(
	               eqFuncName(),
	               IntegerType::getInt1Ty(context),
	               IntegerType::getInt8PtrTy(context),
	               seqIntLLVM(context),
	               IntegerType::getInt8PtrTy(context),
	               seqIntLLVM(context)));

	eq->setLinkage(GlobalValue::PrivateLinkage);
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
	Value *seq1 = memb(self, "ptr", block);
	Value *len1 = memb(self, "len", block);
	Value *seq2 = memb(other, "ptr", block);
	Value *len2 = memb(other, "len", block);

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

	auto *copyFunc = cast<Function>(
	                   block->getModule()->getOrInsertFunction(
	                     getVTable().copyName,
	                     IntegerType::getInt8PtrTy(context),
	                     IntegerType::getInt8PtrTy(context),
	                     seqIntLLVM(context)));

	copyFunc->setCallingConv(CallingConv::C);

	Value *ptr = memb(self, "ptr", block);
	Value *len = memb(self, "len", block);

	IRBuilder<> builder(block);
	Value *copy = builder.CreateCall(copyFunc, {ptr, len});
	return make(copy, len, block);
}

void types::BaseSeqType::serialize(BaseFunc *base,
                                   Value *self,
                                   Value *fp,
                                   BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	Module *module = block->getModule();

	auto *writeFunc = cast<Function>(
	                    module->getOrInsertFunction(
	                      "seq_io_write",
	                      llvm::Type::getVoidTy(context),
	                      IntegerType::getInt8PtrTy(context),
	                      seqIntLLVM(context),
	                      seqIntLLVM(context),
	                      IntegerType::getInt8PtrTy(context)));

	writeFunc->setCallingConv(CallingConv::C);

	Value *ptr = memb(self, "ptr", block);
	Value *len = memb(self, "len", block);
	Int->serialize(base, len, fp, block);

	IRBuilder<> builder(block);
	builder.CreateCall(writeFunc, {ptr, len, oneLLVM(context), fp});
}

Value *types::BaseSeqType::deserialize(BaseFunc *base,
                                       Value *fp,
                                       BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	Module *module = block->getModule();

	auto *readFunc = cast<Function>(
	                   module->getOrInsertFunction(
	                     "seq_io_read",
	                     llvm::Type::getVoidTy(context),
	                     IntegerType::getInt8PtrTy(context),
	                     seqIntLLVM(context),
	                     seqIntLLVM(context),
	                     IntegerType::getInt8PtrTy(context)));

	auto *allocFunc = cast<Function>(
	                    module->getOrInsertFunction(
	                      allocFuncName(),
	                      IntegerType::getInt8PtrTy(context),
	                      IntegerType::getIntNTy(context, sizeof(size_t)*8)));

	readFunc->setCallingConv(CallingConv::C);

	IRBuilder<> builder(block);

	Value *len = Int->deserialize(base, fp, block);
	Value *ptr = builder.CreateCall(allocFunc, {len});
	builder.CreateCall(readFunc, {ptr, len, oneLLVM(context), fp});
	return make(ptr, len, block);
}

void types::BaseSeqType::print(BaseFunc *base,
                               Value *self,
                               BasicBlock *block)
{
	LLVMContext& context = block->getContext();

	auto *printFunc = cast<Function>(
	                    block->getModule()->getOrInsertFunction(
	                      getVTable().printName,
	                      llvm::Type::getVoidTy(context),
	                      IntegerType::getInt8PtrTy(context),
	                      seqIntLLVM(context)));

	printFunc->setCallingConv(CallingConv::C);

	Value *ptr = memb(self, "ptr", block);
	Value *len = memb(self, "len", block);

	IRBuilder<> builder(block);
	builder.CreateCall(printFunc, {ptr, len});
}

Value *types::BaseSeqType::indexLoad(BaseFunc *base,
                                     Value *self,
                                     Value *idx,
                                     BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	Value *ptr = memb(self, "ptr", block);
	IRBuilder<> builder(block);
	ptr = builder.CreateGEP(ptr, idx);
	return make(ptr, oneLLVM(context), block);
}

Value *types::BaseSeqType::indexSlice(BaseFunc *base,
                                      Value *self,
                                      Value *from,
                                      Value *to,
                                      BasicBlock *block)
{
	Value *ptr = memb(self, "ptr", block);
	IRBuilder<> builder(block);
	ptr = builder.CreateGEP(ptr, from);
	Value *len = builder.CreateSub(to, from);
	return make(ptr, len, block);
}

Value *types::BaseSeqType::indexSliceNoFrom(BaseFunc *base,
                                            Value *self,
                                            Value *to,
                                            BasicBlock *block)
{
	Value *zero = zeroLLVM(block->getContext());
	return indexSlice(base, self, zero, to, block);
}

Value *types::BaseSeqType::indexSliceNoTo(BaseFunc *base,
                                          Value *self,
                                          Value *from,
                                          BasicBlock *block)
{
	Value *len = memb(self, "len", block);
	return indexSlice(base, self, from, len, block);
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
		{"len", {0, Int}},
		{"ptr", {1, Void}}
	};
}

bool types::BaseSeqType::isAtomic() const
{
	return false;
}

Type *types::BaseSeqType::getLLVMType(LLVMContext& context) const
{
	return StructType::get(seqIntLLVM(context), IntegerType::getInt8PtrTy(context));
}

seq_int_t types::BaseSeqType::size(Module *module) const
{
	return module->getDataLayout().getTypeAllocSize(getLLVMType(module->getContext()));
}

/* derived Seq type */
types::SeqType::SeqType() : BaseSeqType("Seq")
{
	addMethod("copy", new BaseFuncLite({this}, this, [this](Module *module) {
		LLVMContext& context = module->getContext();
		auto *f = cast<Function>(module->getOrInsertFunction("seq.seq.copy",
		                                                     getLLVMType(context),
		                                                     getLLVMType(context)));
		f->setLinkage(GlobalValue::PrivateLinkage);

		auto *allocFunc = cast<Function>(
		                    module->getOrInsertFunction(
		                      "seq_alloc_atomic",
		                      IntegerType::getInt8PtrTy(context),
		                      IntegerType::getIntNTy(context, sizeof(size_t)*8)));

		Value *arg = f->arg_begin();

		BasicBlock *entry = BasicBlock::Create(context, "entry", f);
		Value *ptr = memb(arg, "ptr", entry);
		Value *len = memb(arg, "len", entry);

		IRBuilder<> builder(entry);
		Value *ptrCopy = builder.CreateCall(allocFunc, len);
		makeMemCpy(ptrCopy, ptr, len, entry, 1);
		Value *copy = make(ptrCopy, len, entry);
		builder.CreateRet(copy);
		return f;
	}), false);
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

void types::SeqType::indexStore(BaseFunc *base,
                                Value *self,
                                Value *idx,
                                Value *val,
                                BasicBlock *block)
{
	Value *dest = memb(self, "ptr", block);
	Value *source = memb(val, "ptr", block);
	Value *len = memb(val, "len", block);

	IRBuilder<> builder(block);
	dest = builder.CreateGEP(dest, idx);
	makeMemMove(dest, source, len, block, 1);
}

void types::SeqType::initOps()
{
	if (!vtable.ops.empty())
		return;

	vtable.ops = {
		{bop("=="), this, Bool, [this](Value *lhs, Value *rhs, IRBuilder<>& b) {
			Value *x = eq(nullptr, lhs, rhs, b.GetInsertBlock());
			return b.CreateZExt(x, Bool->getLLVMType(b.getContext()));
		}},
	};
}

types::Type *types::SeqType::indexType() const
{
	return SeqType::get();
}

types::Type *types::SeqType::subscriptType() const
{
	return types::Int;
}

Value *types::SeqType::make(Value *ptr, Value *len, BasicBlock *block)
{
	LLVMContext& context = ptr->getContext();
	Value *self = UndefValue::get(getLLVMType(context));
	self = setMemb(self, "ptr", ptr, block);
	self = setMemb(self, "len", len, block);
	return self;
}

types::SeqType *types::SeqType::get() noexcept
{
	static types::SeqType instance;
	return &instance;
}

/* derived Str type */
types::StrType::StrType() : BaseSeqType("Str")
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

void types::StrType::initOps()
{
	if (!vtable.ops.empty())
		return;

	vtable.ops = {
		{bop("=="), this, Bool, [this](Value *lhs, Value *rhs, IRBuilder<>& b) {
			Value *x = eq(nullptr, lhs, rhs, b.GetInsertBlock());
			return b.CreateZExt(x, Bool->getLLVMType(b.getContext()));
		}},
	};
}

types::Type *types::StrType::indexType() const
{
	return StrType::get();
}

types::Type *types::StrType::subscriptType() const
{
	return types::Int;
}

Value *types::StrType::make(Value *ptr, Value *len, BasicBlock *block)
{
	LLVMContext& context = ptr->getContext();
	Value *self = UndefValue::get(getLLVMType(context));
	self = setMemb(self, "ptr", ptr, block);
	self = setMemb(self, "len", len, block);
	return self;
}

types::StrType *types::StrType::get() noexcept
{
	static types::StrType instance;
	return &instance;
}
