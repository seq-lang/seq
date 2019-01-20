#include <iostream>
#include "seq/seq.h"

using namespace seq;
using namespace llvm;

types::BaseSeqType::BaseSeqType(std::string name) :
    Type(std::move(name), BaseType::get(), false, true)
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

size_t types::BaseSeqType::size(Module *module) const
{
	return module->getDataLayout().getTypeAllocSize(getLLVMType(module->getContext()));
}

/* derived Seq type */
types::SeqType::SeqType() : BaseSeqType("seq")
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
			printFunc->setDoesNotThrow();
			b.CreateCall(printFunc, self);
			return (Value *)nullptr;
		}},

		{"__str__", {}, Str, SEQ_MAGIC(self, args, b) {
			return self;
		}},

		{"__copy__", {}, Seq, SEQ_MAGIC_CAPT(self, args, b) {
			BasicBlock *block = b.GetInsertBlock();
			Module *module = block->getModule();
			auto *allocFunc = makeAllocFunc(module, true);
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
types::StrType::StrType() : BaseSeqType("str")
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
			printFunc->setDoesNotThrow();
			b.CreateCall(printFunc, self);
			return (Value *)nullptr;
		}},

		{"__str__", {}, Str, SEQ_MAGIC(self, args, b) {
			return self;
		}},

		{"__copy__", {}, Str, SEQ_MAGIC(self, args, b) {
			return self;
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


/*
 * k-mer types (fixed-length short sequences)
 */

#define KMER_MAX_LEN 128

types::KMer::KMer(unsigned k) :
    Type("k-mer", BaseType::get(), false, true), k(k)
{
	if (k == 0 || k > KMER_MAX_LEN)
		throw exc::SeqException("k-mer length must be between 1 and " + std::to_string(KMER_MAX_LEN));

	addMethod("len", new BaseFuncLite({}, types::IntType::get(), [this](Module *module) {
		const std::string name = "seq." + getName() + ".len";
		Function *func = module->getFunction(name);

		if (!func) {
			LLVMContext& context = module->getContext();
			func = cast<Function>(module->getOrInsertFunction(name, seqIntLLVM(context)));
			func->setDoesNotThrow();
			func->setLinkage(GlobalValue::PrivateLinkage);
			AttributeList v;
			v.addAttribute(context, 0, Attribute::AlwaysInline);
			func->setAttributes(v);
			BasicBlock *block = BasicBlock::Create(context, "entry", func);
			IRBuilder<> builder(block);
			builder.CreateRet(ConstantInt::get(seqIntLLVM(context), this->k));
		}

		return func;
	}), true);
}

unsigned types::KMer::getK()
{
	return k;
}

std::string types::KMer::getName() const
{
	return std::to_string(k) + "-mer";
}

Value *types::KMer::defaultValue(BasicBlock *block)
{
	return ConstantInt::get(getLLVMType(block->getContext()), 0);
}

static GlobalVariable *get2bitTable(Module *module, const std::string& name="seq.2bit_table")
{
	LLVMContext& context = module->getContext();
	Type *ty = IntegerType::getIntNTy(context, 2);

	GlobalVariable *table = module->getGlobalVariable(name);

	if (!table) {
		std::vector<Constant *> v(256, ConstantInt::get(ty, 0));
		v['A'] = v['a'] = ConstantInt::get(ty, 0);
		v['C'] = v['c'] = ConstantInt::get(ty, 1);
		v['G'] = v['g'] = ConstantInt::get(ty, 2);
		v['T'] = v['t'] = ConstantInt::get(ty, 3);

		auto *arrTy = llvm::ArrayType::get(IntegerType::getIntNTy(context, 2), 256);
		table = new GlobalVariable(*module,
		                           arrTy,
		                           true,
		                           GlobalValue::PrivateLinkage,
		                           ConstantArray::get(arrTy, v),
		                           name);
	}

	return table;
}

/*
 * Here we create functions for k-mer sliding window shifts. For example, if some
 * k-mer represents a portion of a longer read, we can "shift in" the next base
 * of the sequence via `kmer >> read[i]`, or equivalently in the other direction.
 *
 * dir=false for left; dir=true for right
 */
static Function *getShiftFunc(types::KMer *kmerType, Module *module, bool dir)
{
	const std::string name = "seq." + kmerType->getName() + ".sh" + (dir ? "r" : "l");
	LLVMContext& context = module->getContext();
	Function *func = module->getFunction(name);

	if (!func) {
		GlobalVariable *table = get2bitTable(module);

		func = cast<Function>(
		         module->getOrInsertFunction(name,
		                                     kmerType->getLLVMType(context),
		                                     kmerType->getLLVMType(context),
		                                     types::Seq->getLLVMType(context)));
		func->setDoesNotThrow();
		func->setLinkage(GlobalValue::PrivateLinkage);
		AttributeList v;
		v.addAttribute(context, 0, Attribute::AlwaysInline);
		func->setAttributes(v);

		auto iter = func->arg_begin();
		Value *kmer = iter++;
		Value *seq = iter;

		/*
		 * The following function is just a for-loop that continually shifts in
		 * new bases from the given sequences into the k-mer, then returns it.
		 */
		BasicBlock *entry = BasicBlock::Create(context, "entry", func);
		Value *ptr = types::Seq->memb(seq, "ptr", entry);
		Value *len = types::Seq->memb(seq, "len", entry);
		IRBuilder<> builder(entry);

		BasicBlock *loop = BasicBlock::Create(context, "while", func);
		builder.CreateBr(loop);
		builder.SetInsertPoint(loop);

		PHINode *control = builder.CreatePHI(seqIntLLVM(context), 2);
		PHINode *result = builder.CreatePHI(kmerType->getLLVMType(context), 2);
		control->addIncoming(zeroLLVM(context), entry);
		result->addIncoming(kmer, entry);
		Value *cond = builder.CreateICmpSLT(control, len);

		BasicBlock *body = BasicBlock::Create(context, "body", func);
		BranchInst *branch = builder.CreateCondBr(cond, body, body);  // we set false-branch below

		builder.SetInsertPoint(body);
		Value *kmerMod = nullptr;

		if (dir) {
			// right slide
			kmerMod = builder.CreateLShr(result, 2);
			Value *base = builder.CreateLoad(builder.CreateGEP(ptr, control));
			base = builder.CreateZExt(base, builder.getInt64Ty());
			Value *bits = builder.CreateLoad(builder.CreateGEP(table, {builder.getInt64(0), base}));
			bits = builder.CreateZExt(bits, kmerType->getLLVMType(context));
			bits = builder.CreateShl(bits, 2*(kmerType->getK() - 1));
			kmerMod = builder.CreateOr(kmerMod, bits);
		} else {
			// left slide
			Value *idx = builder.CreateSub(len, oneLLVM(context));
			idx = builder.CreateSub(idx, control);
			kmerMod = builder.CreateShl(result, 2);
			Value *base = builder.CreateLoad(builder.CreateGEP(ptr, idx));
			base = builder.CreateZExt(base, builder.getInt64Ty());
			Value *bits = builder.CreateLoad(builder.CreateGEP(table, {builder.getInt64(0), base}));
			bits = builder.CreateZExt(bits, kmerType->getLLVMType(context));
			kmerMod = builder.CreateOr(kmerMod, bits);
		}

		Value *next = builder.CreateAdd(control, oneLLVM(context));
		control->addIncoming(next, body);
		result->addIncoming(kmerMod, body);
		builder.CreateBr(loop);

		BasicBlock *exit = BasicBlock::Create(context, "exit", func);
		branch->setSuccessor(1, exit);
		builder.SetInsertPoint(exit);
		builder.CreateRet(result);
	}

	return func;
}

void types::KMer::initOps()
{
	if (!vtable.magic.empty())
		return;

	vtable.magic = {
		{"__init__", {Seq}, this, SEQ_MAGIC_CAPT(self, args, b) {
			LLVMContext& context = b.getContext();
			BasicBlock *block = b.GetInsertBlock();
			Module *module = block->getModule();

			GlobalVariable *table = get2bitTable(module);
			Value *ptr  = Seq->memb(args[0], "ptr", block);
			Value *kmer = defaultValue(block);

			for (unsigned i = 0; i < k; i++) {
				Value *base = b.CreateLoad(b.CreateGEP(ptr, b.getInt64(i)));
				base = b.CreateZExt(base, b.getInt64Ty());
				Value *bits = b.CreateLoad(b.CreateGEP(table, {b.getInt64(0), base}));
				bits = b.CreateZExt(bits, getLLVMType(context));

				Value *shift = b.CreateShl(bits, 2*i);
				kmer = b.CreateOr(kmer, shift);
			}

			return kmer;
		}},

		{"__print__", {}, Void, SEQ_MAGIC_CAPT(self, args, b) {
			LLVMContext& context = b.getContext();
			Module *module = b.GetInsertBlock()->getModule();
			auto *printFunc = cast<Function>(
			                    module->getOrInsertFunction(
			                      "seq_print_base_2bit",
			                      llvm::Type::getVoidTy(context),
			                      b.getInt8Ty()));
			printFunc->setDoesNotThrow();

			for (unsigned i = 0; i < k; i++) {
				Value *mask = ConstantInt::get(getLLVMType(context), 3);
				mask = b.CreateShl(mask, 2*i);
				mask = b.CreateAnd(self, mask);
				mask = b.CreateLShr(mask, 2*i);
				mask = b.CreateTrunc(mask, b.getInt8Ty());
				b.CreateCall(printFunc, mask);
			}
			return (Value *)nullptr;
		}},

		{"__copy__", {}, this, SEQ_MAGIC(self, args, b) {
			return self;
		}},

		// reversal
		/* TODO -- use lookup table or some bit hack
		{"__neg__", {}, this, SEQ_MAGIC(self, args, b) {
		}},
		 */

		// complement
		{"__invert__", {}, this, SEQ_MAGIC(self, args, b) {
			return b.CreateNot(self);
		}},

		// slide window left
		{"__lshift__", {Seq}, this, SEQ_MAGIC_CAPT(self, args, b) {
			Module *module = b.GetInsertBlock()->getModule();
			return b.CreateCall(getShiftFunc(this, module, false), {self, args[0]});
		}},

		// slide window right
		{"__rshift__", {Seq}, this, SEQ_MAGIC_CAPT(self, args, b) {
			Module *module = b.GetInsertBlock()->getModule();
			return b.CreateCall(getShiftFunc(this, module, true), {self, args[0]});
		}},

		{"__hash__", {}, Int, SEQ_MAGIC(self, args, b) {
			return b.CreateZExtOrTrunc(self, seqIntLLVM(b.getContext()));
		}},

		{"__len__", {}, Int, SEQ_MAGIC_CAPT(self, args, b) {
			return ConstantInt::get(seqIntLLVM(b.getContext()), k, true);
		}},

		{"__eq__", {this}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpEQ(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__ne__", {this}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpNE(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__lt__", {this}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpULT(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__gt__", {this}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpUGT(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__le__", {this}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpULE(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__ge__", {this}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpUGE(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},
	};
}

bool types::KMer::isAtomic() const
{
	return true;
}

bool types::KMer::is(seq::types::Type *type) const
{
	types::KMer *kmer = type->asKMer();
	return kmer && k == kmer->k;
}

Type *types::KMer::getLLVMType(LLVMContext& context) const
{
	return IntegerType::getIntNTy(context, 2*k);
}

size_t types::KMer::size(Module *module) const
{
	return module->getDataLayout().getTypeAllocSize(getLLVMType(module->getContext()));
}

types::KMer *types::KMer::asKMer()
{
	return this;
}

types::KMer *types::KMer::get(unsigned k)
{
	static std::map<unsigned, KMer *> cache;

	if (cache.find(k) != cache.end())
		return cache.find(k)->second;

	auto *kmerType = new KMer(k);
	cache.insert({k, kmerType});
	return kmerType;
}
