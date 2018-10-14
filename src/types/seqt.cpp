#include <iostream>
#include "seq/seq.h"

using namespace seq;
using namespace llvm;

types::BaseSeqType::BaseSeqType(std::string name) :
    Type(std::move(name), BaseType::get())
{
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

Value *types::BaseSeqType::eq(Value *self,
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
		{"ptr", {1, PtrType::get(Byte)}}
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

void types::SeqType::initOps()
{
	if (!vtable.magic.empty())
		return;

	vtable.magic = {
		{"__init__", {PtrType::get(Byte), Int}, Seq, SEQ_MAGIC_CAPT(self, args, b) {
			return make(args[0], args[1], b.GetInsertBlock());
		}},

		{"__print__", {}, Void, SEQ_MAGIC_CAPT(self, args, b) {
			LLVMContext& context = b.getContext();
			Module *module = b.GetInsertBlock()->getModule();
			auto *printFunc = cast<Function>(
			                    module->getOrInsertFunction(
			                      "seq_print_seq",
			                      llvm::Type::getVoidTy(context),
			                      getLLVMType(context)));

			b.CreateCall(printFunc, self);
			return (Value *)nullptr;
		}},

		{"__copy__", {}, Seq, SEQ_MAGIC_CAPT(self, args, b) {
			BasicBlock *block = b.GetInsertBlock();
			Module *module = block->getModule();
			LLVMContext& context = module->getContext();

			auto *allocFunc = cast<Function>(
			                    module->getOrInsertFunction(
			                      "seq_alloc_atomic",
			                      IntegerType::getInt8PtrTy(context),
			                      IntegerType::getIntNTy(context, sizeof(size_t)*8)));

			Value *ptr = memb(self, "ptr", block);
			Value *len = memb(self, "len", block);
			Value *ptrCopy = b.CreateCall(allocFunc, len);
			makeMemCpy(ptrCopy, ptr, len, block, 1);
			Value *copy = make(ptrCopy, len, block);
			return copy;
		}},

		{"__len__", {}, Int, SEQ_MAGIC_CAPT(self, args, b) {
			return memb(self, "len", b.GetInsertBlock());
		}},

		{"__bool__", {}, Bool, SEQ_MAGIC_CAPT(self, args, b) {
			Value *len = memb(self, "len", b.GetInsertBlock());
			Value *zero = ConstantInt::get(Int->getLLVMType(b.getContext()), 0);
			return b.CreateZExt(b.CreateICmpNE(len, zero), Bool->getLLVMType(b.getContext()));
		}},

		{"__getitem__", {Int}, Seq, SEQ_MAGIC_CAPT(self, args, b) {
			Value *ptr = memb(self, "ptr", b.GetInsertBlock());
			ptr = b.CreateGEP(ptr, args[0]);
			return make(ptr, oneLLVM(b.getContext()), b.GetInsertBlock());
		}},

		{"__slice__", {Int, Int}, Seq, SEQ_MAGIC_CAPT(self, args, b) {
			Value *ptr = memb(self, "ptr", b.GetInsertBlock());
			ptr = b.CreateGEP(ptr, args[0]);
			Value *len = b.CreateSub(args[1], args[0]);
			return make(ptr, len, b.GetInsertBlock());
		}},

		{"__slice_left__", {Int}, Seq, SEQ_MAGIC_CAPT(self, args, b) {
			Value *ptr = memb(self, "ptr", b.GetInsertBlock());
			return make(ptr, args[0], b.GetInsertBlock());
		}},

		{"__slice_right__", {Int}, Seq, SEQ_MAGIC_CAPT(self, args, b) {
			Value *ptr = memb(self, "ptr", b.GetInsertBlock());
			Value *to = memb(self, "len", b.GetInsertBlock());
			ptr = b.CreateGEP(ptr, args[0]);
			Value *len = b.CreateSub(to, args[0]);
			return make(ptr, len, b.GetInsertBlock());
		}},

		{"__setitem__", {Int, Seq}, Void, SEQ_MAGIC_CAPT(self, args, b) {
			BasicBlock *block = b.GetInsertBlock();
			Value *dest = memb(self, "ptr", block);
			Value *source = memb(args[1], "ptr", block);
			Value *len = memb(args[1], "len", block);
			dest = b.CreateGEP(dest, args[0]);
			makeMemMove(dest, source, len, block, 1);
			return (Value *)nullptr;
		}},

		{"__eq__", {Seq}, Bool, SEQ_MAGIC_CAPT(self, args, b) {
			Value *x = eq(self, args[0], b.GetInsertBlock());
			return b.CreateZExt(x, Bool->getLLVMType(b.getContext()));
		}},

		{"__ne__", {Seq}, Bool, SEQ_MAGIC_CAPT(self, args, b) {
			Value *x = eq(self, args[0], b.GetInsertBlock());
			x = b.CreateNot(x);
			return b.CreateZExt(x, Bool->getLLVMType(b.getContext()));
		}},
	};
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
	if (!vtable.magic.empty())
		return;

	vtable.magic = {
		{"__init__", {PtrType::get(Byte), Int}, Str, SEQ_MAGIC_CAPT(self, args, b) {
			return make(args[0], args[1], b.GetInsertBlock());
		}},

		{"__print__", {}, Void, SEQ_MAGIC_CAPT(self, args, b) {
			LLVMContext& context = b.getContext();
			Module *module = b.GetInsertBlock()->getModule();
			auto *printFunc = cast<Function>(
			                    module->getOrInsertFunction(
			                      "seq_print_str",
			                      llvm::Type::getVoidTy(context),
			                      getLLVMType(context)));

			b.CreateCall(printFunc, self);
			return (Value *)nullptr;
		}},

		{"__copy__", {}, Str, SEQ_MAGIC_CAPT(self, args, b) {
			BasicBlock *block = b.GetInsertBlock();
			Module *module = block->getModule();
			LLVMContext& context = module->getContext();

			auto *allocFunc = cast<Function>(
			                    module->getOrInsertFunction(
			                      "seq_alloc_atomic",
			                      IntegerType::getInt8PtrTy(context),
			                      IntegerType::getIntNTy(context, sizeof(size_t)*8)));

			Value *ptr = memb(self, "ptr", block);
			Value *len = memb(self, "len", block);
			Value *ptrCopy = b.CreateCall(allocFunc, len);
			makeMemCpy(ptrCopy, ptr, len, block, 1);
			Value *copy = make(ptrCopy, len, block);
			return copy;
		}},

		{"__len__", {}, Int, SEQ_MAGIC_CAPT(self, args, b) {
			return memb(self, "len", b.GetInsertBlock());
		}},

		{"__bool__", {}, Bool, SEQ_MAGIC_CAPT(self, args, b) {
			Value *len = memb(self, "len", b.GetInsertBlock());
			Value *zero = ConstantInt::get(Int->getLLVMType(b.getContext()), 0);
			return b.CreateZExt(b.CreateICmpNE(len, zero), Bool->getLLVMType(b.getContext()));
		}},

		{"__getitem__", {Int}, Str, SEQ_MAGIC_CAPT(self, args, b) {
			Value *ptr = memb(self, "ptr", b.GetInsertBlock());
			ptr = b.CreateGEP(ptr, args[0]);
			return make(ptr, oneLLVM(b.getContext()), b.GetInsertBlock());
		}},

		{"__slice__", {Int, Int}, Str, SEQ_MAGIC_CAPT(self, args, b) {
			Value *ptr = memb(self, "ptr", b.GetInsertBlock());
			ptr = b.CreateGEP(ptr, args[0]);
			Value *len = b.CreateSub(args[1], args[0]);
			return make(ptr, len, b.GetInsertBlock());
		}},

		{"__slice_left__", {Int}, Str, SEQ_MAGIC_CAPT(self, args, b) {
			Value *ptr = memb(self, "ptr", b.GetInsertBlock());
			return make(ptr, args[0], b.GetInsertBlock());
		}},

		{"__slice_right__", {Int}, Str, SEQ_MAGIC_CAPT(self, args, b) {
			Value *ptr = memb(self, "ptr", b.GetInsertBlock());
			Value *to = memb(self, "len", b.GetInsertBlock());
			ptr = b.CreateGEP(ptr, args[0]);
			Value *len = b.CreateSub(to, args[0]);
			return make(ptr, len, b.GetInsertBlock());
		}},

		{"__eq__", {Str}, Bool, SEQ_MAGIC_CAPT(self, args, b) {
			Value *x = eq(self, args[0], b.GetInsertBlock());
			return b.CreateZExt(x, Bool->getLLVMType(b.getContext()));
		}},

		{"__ne__", {Str}, Bool, SEQ_MAGIC_CAPT(self, args, b) {
			Value *x = eq(self, args[0], b.GetInsertBlock());
			x = b.CreateNot(x);
			return b.CreateZExt(x, Bool->getLLVMType(b.getContext()));
		}},
	};
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
