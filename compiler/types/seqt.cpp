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
		}, false},

		{"__str__", {}, Str, SEQ_MAGIC(self, args, b) {
			return self;
		}, false},

		{"__len__", {}, Int, SEQ_MAGIC_CAPT(self, args, b) {
			return memb(self, "len", b.GetInsertBlock());
		}, false},

		{"__bool__", {}, Bool, SEQ_MAGIC_CAPT(self, args, b) {
			Value *len = memb(self, "len", b.GetInsertBlock());
			Value *zero = ConstantInt::get(Int->getLLVMType(b.getContext()), 0);
			return b.CreateZExt(b.CreateICmpNE(len, zero), Bool->getLLVMType(b.getContext()));
		}, false},

		{"__setitem__", {Int, Seq}, Void, SEQ_MAGIC_CAPT(self, args, b) {
			BasicBlock *block = b.GetInsertBlock();
			Value *dest = memb(self, "ptr", block);
			Value *source = memb(args[1], "ptr", block);
			Value *len = memb(args[1], "len", block);
			dest = b.CreateGEP(dest, args[0]);
			makeMemMove(dest, source, len, block, 1);
			return (Value *)nullptr;
		}, false},

		{"__eq__", {Seq}, Bool, SEQ_MAGIC_CAPT(self, args, b) {
			Value *x = eq(self, args[0], b.GetInsertBlock());
			return b.CreateZExt(x, Bool->getLLVMType(b.getContext()));
		}, false},

		{"__ne__", {Seq}, Bool, SEQ_MAGIC_CAPT(self, args, b) {
			Value *x = eq(self, args[0], b.GetInsertBlock());
			x = b.CreateNot(x);
			return b.CreateZExt(x, Bool->getLLVMType(b.getContext()));
		}, false},
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
		}, false},

		{"__str__", {}, Str, SEQ_MAGIC(self, args, b) {
			return self;
		}, false},

		{"__copy__", {}, Str, SEQ_MAGIC(self, args, b) {
			return self;
		}, false},

		{"__len__", {}, Int, SEQ_MAGIC_CAPT(self, args, b) {
			return memb(self, "len", b.GetInsertBlock());
		}, false},

		{"__bool__", {}, Bool, SEQ_MAGIC_CAPT(self, args, b) {
			Value *len = memb(self, "len", b.GetInsertBlock());
			Value *zero = ConstantInt::get(Int->getLLVMType(b.getContext()), 0);
			return b.CreateZExt(b.CreateICmpNE(len, zero), Bool->getLLVMType(b.getContext()));
		}, false},

		{"__eq__", {Str}, Bool, SEQ_MAGIC_CAPT(self, args, b) {
			Value *x = eq(self, args[0], b.GetInsertBlock());
			return b.CreateZExt(x, Bool->getLLVMType(b.getContext()));
		}, false},

		{"__ne__", {Str}, Bool, SEQ_MAGIC_CAPT(self, args, b) {
			Value *x = eq(self, args[0], b.GetInsertBlock());
			x = b.CreateNot(x);
			return b.CreateZExt(x, Bool->getLLVMType(b.getContext()));
		}, false},
	};

	addMethod("memcpy", new BaseFuncLite({PtrType::get(Byte), PtrType::get(Byte), Int},
	                                     Void,
	                                     [](Module *module) {
		const std::string name = "seq.memcpy";
		Function *func = module->getFunction(name);

		if (!func) {
			LLVMContext& context = module->getContext();
			func = cast<Function>(module->getOrInsertFunction(name,
			                                                  llvm::Type::getVoidTy(context),
			                                                  IntegerType::getInt8PtrTy(context),
			                                                  IntegerType::getInt8PtrTy(context),
			                                                  seqIntLLVM(context)));
			func->setDoesNotThrow();
			func->setLinkage(GlobalValue::PrivateLinkage);
			func->addFnAttr(Attribute::AlwaysInline);
			auto iter = func->arg_begin();
			Value *dst = iter++;
			Value *src = iter++;
			Value *len = iter;
			BasicBlock *block = BasicBlock::Create(context, "entry", func);
			makeMemCpy(dst, src, len, block);
			IRBuilder<> builder(block);
			builder.CreateRetVoid();
		}

		return func;
	}), true);
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

types::KMer::KMer(unsigned k) :
    Type("k-mer", BaseType::get(), false, true), k(k)
{
	if (k == 0 || k > MAX_LEN)
		throw exc::SeqException("k-mer length must be between 1 and " + std::to_string(MAX_LEN));
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

// table mapping ASCII characters to 2-bit encodings
static GlobalVariable *get2bitTable(Module *module, const std::string& name="seq.2bit_table")
{
	LLVMContext& context = module->getContext();
	Type *ty = IntegerType::getIntNTy(context, 2);
	GlobalVariable *table = module->getGlobalVariable(name);

	if (!table) {
		std::vector<Constant *> v(256, ConstantInt::get(ty, 0));
		v['A'] = v['a'] = ConstantInt::get(ty, 0);
		v['G'] = v['g'] = ConstantInt::get(ty, 1);
		v['C'] = v['c'] = ConstantInt::get(ty, 2);
		v['T'] = v['t'] = ConstantInt::get(ty, 3);

		auto *arrTy = llvm::ArrayType::get(IntegerType::getIntNTy(context, 2), v.size());
		table = new GlobalVariable(*module,
		                           arrTy,
		                           true,
		                           GlobalValue::PrivateLinkage,
		                           ConstantArray::get(arrTy, v),
		                           name);
	}

	return table;
}

// table mapping 2-bit encodings to ASCII characters
static GlobalVariable *get2bitTableInv(Module *module, const std::string& name="seq.2bit_table_inv")
{
	LLVMContext& context = module->getContext();
	GlobalVariable *table = module->getGlobalVariable(name);

	if (!table) {
		table = new GlobalVariable(*module,
		                           llvm::ArrayType::get(IntegerType::getInt8Ty(context), 4),
		                           true,
		                           GlobalValue::PrivateLinkage,
		                           ConstantDataArray::getString(context, "AGCT", false),
		                           name);
	}

	return table;
}

static unsigned revcompBits(unsigned n)
{
	unsigned c1 = (n & (3u << 0u)) << 6u;
	unsigned c2 = (n & (3u << 2u)) << 2u;
	unsigned c3 = (n & (3u << 4u)) >> 2u;
	unsigned c4 = (n & (3u << 6u)) >> 6u;
	return ~(c1 | c2 | c3 | c4) & 0xffu;
}

// table mapping 8-bit encoded 4-mers to reverse complement encoded 4-mers
static GlobalVariable *getRevCompTable(Module *module, const std::string& name="seq.revcomp_table")
{
	LLVMContext& context = module->getContext();
	Type *ty = IntegerType::getInt8Ty(context);
	GlobalVariable *table = module->getGlobalVariable(name);

	if (!table) {
		std::vector<Constant *> v(256, ConstantInt::get(ty, 0));
		for (unsigned i = 0; i < v.size(); i++)
			v[i] = ConstantInt::get(ty, revcompBits(i));

		auto *arrTy = llvm::ArrayType::get(IntegerType::getInt8Ty(context), v.size());
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
		func->addFnAttr(Attribute::AlwaysInline);

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
			Value *idx = builder.CreateSub(len, oneLLVM(context));
			idx = builder.CreateSub(idx, control);
			kmerMod = builder.CreateLShr(result, 2);
			Value *base = builder.CreateLoad(builder.CreateGEP(ptr, idx));
			base = builder.CreateZExt(base, builder.getInt64Ty());
			Value *bits = builder.CreateLoad(builder.CreateInBoundsGEP(table, {builder.getInt64(0), base}));
			bits = builder.CreateZExt(bits, kmerType->getLLVMType(context));
			bits = builder.CreateShl(bits, 2*(kmerType->getK() - 1));
			kmerMod = builder.CreateOr(kmerMod, bits);
		} else {
			// left slide
			kmerMod = builder.CreateShl(result, 2);
			Value *base = builder.CreateLoad(builder.CreateGEP(ptr, control));
			base = builder.CreateZExt(base, builder.getInt64Ty());
			Value *bits = builder.CreateLoad(builder.CreateInBoundsGEP(table, {builder.getInt64(0), base}));
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

/*
 * Seq-to-KMer initialization function
 */
static Function *getInitFunc(types::KMer *kmerType, Module *module)
{
	const std::string name = "seq." + kmerType->getName() + ".init";
	LLVMContext& context = module->getContext();
	Function *func = module->getFunction(name);

	if (!func) {
		func = cast<Function>(
		         module->getOrInsertFunction(name,
		                                     kmerType->getLLVMType(context),
		                                     types::Seq->getLLVMType(context)));
		func->setDoesNotThrow();
		func->setLinkage(GlobalValue::PrivateLinkage);

		Value *seq = func->arg_begin();
		BasicBlock *block = BasicBlock::Create(context, "entry", func);
		IRBuilder<> builder(block);

		const unsigned k = kmerType->getK();
		GlobalVariable *table = get2bitTable(module);
		Value *ptr  = types::Seq->memb(seq, "ptr", block);
		Value *kmer = kmerType->defaultValue(block);

		for (unsigned i = 0; i < k; i++) {
			Value *base = builder.CreateLoad(builder.CreateGEP(ptr, builder.getInt64(i)));
			base = builder.CreateZExt(base, builder.getInt64Ty());
			Value *bits = builder.CreateLoad(
			                builder.CreateInBoundsGEP(table, {builder.getInt64(0), base}));
			bits = builder.CreateZExt(bits, kmerType->getLLVMType(context));

			Value *shift = builder.CreateShl(bits, (k - i - 1)*2);
			kmer = builder.CreateOr(kmer, shift);
		}

		builder.CreateRet(kmer);
	}

	return func;
}

/*
 * KMer-to-Str conversion function
 */
static Function *getStrFunc(types::KMer *kmerType, Module *module)
{
	const std::string name = "seq." + kmerType->getName() + ".str";
	LLVMContext& context = module->getContext();
	Function *func = module->getFunction(name);

	if (!func) {
		func = cast<Function>(
		         module->getOrInsertFunction(name,
		                                     types::Str->getLLVMType(context),
		                                     kmerType->getLLVMType(context)));
		func->setDoesNotThrow();
		func->setLinkage(GlobalValue::PrivateLinkage);

		Value *kmer = func->arg_begin();
		BasicBlock *block = BasicBlock::Create(context, "entry", func);
		IRBuilder<> builder(block);

		const unsigned k = kmerType->getK();
		GlobalVariable *table = get2bitTableInv(module);
		Value *len = ConstantInt::get(seqIntLLVM(context), k);
		Value *buf = types::Byte->alloc(len, block);

		for (unsigned i = 0; i < k; i++) {
			Value *mask = ConstantInt::get(kmerType->getLLVMType(context), 3);
			unsigned shift = (k - i - 1)*2;
			mask = builder.CreateShl(mask, shift);
			mask = builder.CreateAnd(kmer, mask);
			mask = builder.CreateLShr(mask, shift);
			mask = builder.CreateZExtOrTrunc(mask, builder.getInt64Ty());
			Value *base = builder.CreateInBoundsGEP(table, {builder.getInt64(0), mask});
			base = builder.CreateLoad(base);
			Value *dest = builder.CreateGEP(buf, builder.getInt32(i));
			builder.CreateStore(base, dest);
		}

		builder.CreateRet(types::Str->make(buf, len, block));
	}

	return func;
}

void types::KMer::initOps()
{
	if (!vtable.magic.empty())
		return;

	vtable.magic = {
		{"__init__", {Seq}, this, SEQ_MAGIC_CAPT(self, args, b) {
			Function *initFunc = getInitFunc(this, b.GetInsertBlock()->getModule());
			return b.CreateCall(initFunc, args[0]);
		}, false},

		{"__init__", {IntNType::get(2*k, false)}, this, SEQ_MAGIC_CAPT(self, args, b) {
			return b.CreateBitCast(args[0], getLLVMType(b.getContext()));
		}, false},

		{"__init__", {Int}, this, SEQ_MAGIC_CAPT(self, args, b) {
			return b.CreateZExtOrTrunc(args[0], getLLVMType(b.getContext()));
		}, false},

		{"__str__", {}, Str, SEQ_MAGIC_CAPT(self, args, b) {
			Function *strFunc = getStrFunc(this, b.GetInsertBlock()->getModule());
			return b.CreateCall(strFunc, self);
		}, false},

		{"__copy__", {}, this, SEQ_MAGIC(self, args, b) {
			return self;
		}, false},

		// reverse complement
		{"__invert__", {}, this, SEQ_MAGIC_CAPT(self, args, b) {
			LLVMContext& context = b.getContext();
			Module *module = b.GetInsertBlock()->getModule();
			Value *table = getRevCompTable(module);
			Value *mask = ConstantInt::get(getLLVMType(context), 0xffu);
			Value *result = ConstantInt::get(getLLVMType(context), 0);

			// deal with 8-bit chunks:
			for (unsigned i = 0; i < k/4; i++) {
				Value *slice = b.CreateShl(mask, i*8);
				slice = b.CreateAnd(self, slice);
				slice = b.CreateLShr(slice, i*8);
				slice = b.CreateZExtOrTrunc(slice, b.getInt64Ty());

				Value *sliceRC = b.CreateInBoundsGEP(table, {b.getInt64(0), slice});
				sliceRC = b.CreateLoad(sliceRC);
				sliceRC = b.CreateZExtOrTrunc(sliceRC, getLLVMType(context));
				sliceRC = b.CreateShl(sliceRC, (k - 4*(i+1))*2);
				result = b.CreateOr(result, sliceRC);
			}

			// deal with remaining high bits:
			unsigned rem = k % 4;
			if (rem > 0) {
				mask = ConstantInt::get(getLLVMType(context), (1u << (rem*2)) - 1);
				Value *slice = b.CreateShl(mask, (k - rem)*2);
				slice = b.CreateAnd(self, slice);
				slice = b.CreateLShr(slice, (k - rem)*2);
				slice = b.CreateZExtOrTrunc(slice, b.getInt64Ty());

				Value *sliceRC = b.CreateInBoundsGEP(table, {b.getInt64(0), slice});
				sliceRC = b.CreateLoad(sliceRC);
				sliceRC = b.CreateAShr(sliceRC, (4 - rem)*2);  // slice isn't full 8-bits, so shift out junk
				sliceRC = b.CreateZExtOrTrunc(sliceRC, getLLVMType(context));
				sliceRC = b.CreateAnd(sliceRC, mask);
				result = b.CreateOr(result, sliceRC);
			}

			return result;
		}, false},

		// slide window left
		{"__lshift__", {Seq}, this, SEQ_MAGIC_CAPT(self, args, b) {
			Module *module = b.GetInsertBlock()->getModule();
			return b.CreateCall(getShiftFunc(this, module, false), {self, args[0]});
		}, false},

		// slide window right
		{"__rshift__", {Seq}, this, SEQ_MAGIC_CAPT(self, args, b) {
			Module *module = b.GetInsertBlock()->getModule();
			return b.CreateCall(getShiftFunc(this, module, true), {self, args[0]});
		}, false},

		// Hamming distance
		{"__sub__", {this}, Int, SEQ_MAGIC_CAPT(self, args, b) {
			/*
			 * Hamming distance algorithm:
			 *   input: kmer1, kmer2
			 *   mask1 = 0101...0101  (same bit width as encoded kmer)
			 *   mask2 = 1010...1010  (same bit width as encoded kmer)
			 *   popcnt(
			 *     (((kmer1 & mask1) ^ (kmer2 & mask1)) << 1) |
			 *     ((kmer1 & mask2) ^ (kmer2 & mask2))
			 *   )
			 */
			LLVMContext& context = b.getContext();
			Value *mask1 = ConstantInt::get(getLLVMType(context), 0);
			for (unsigned i = 0; i < getK(); i++) {
				Value *shift = b.CreateShl(oneLLVM(context), 2 * i);
				mask1 = b.CreateOr(mask1, shift);
			}
			Value *mask2 = b.CreateShl(mask1, 1);

			Value *k1m1 = b.CreateAnd(self, mask1);
			Value *k1m2 = b.CreateAnd(self, mask2);
			Value *k2m1 = b.CreateAnd(args[0], mask1);
			Value *k2m2 = b.CreateAnd(args[0], mask2);
			Value *xor1 = b.CreateShl(b.CreateXor(k1m1, k2m1), 1);
			Value *xor2 = b.CreateXor(k1m2, k2m2);
			Value *diff = b.CreateOr(xor1, xor2);

			Function *popcnt = Intrinsic::getDeclaration(b.GetInsertBlock()->getModule(), Intrinsic::ctpop, {getLLVMType(context)});
			Value *result = b.CreateCall(popcnt, diff);
			return b.CreateZExtOrTrunc(result, seqIntLLVM(context));
		}, false},

		{"__hash__", {}, Int, SEQ_MAGIC(self, args, b) {
			return b.CreateZExtOrTrunc(self, seqIntLLVM(b.getContext()));
		}, false},

		{"__len__", {}, Int, SEQ_MAGIC_CAPT(self, args, b) {
			return ConstantInt::get(seqIntLLVM(b.getContext()), k, true);
		}, false},

		{"__eq__", {this}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpEQ(self, args[0]), Bool->getLLVMType(b.getContext()));
		}, false},

		{"__ne__", {this}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpNE(self, args[0]), Bool->getLLVMType(b.getContext()));
		}, false},

		{"__lt__", {this}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpULT(self, args[0]), Bool->getLLVMType(b.getContext()));
		}, false},

		{"__gt__", {this}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpUGT(self, args[0]), Bool->getLLVMType(b.getContext()));
		}, false},

		{"__le__", {this}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpULE(self, args[0]), Bool->getLLVMType(b.getContext()));
		}, false},

		{"__ge__", {this}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpUGE(self, args[0]), Bool->getLLVMType(b.getContext()));
		}, false},
	};

	addMethod("len", new BaseFuncLite({}, types::IntType::get(), [this](Module *module) {
		const std::string name = "seq." + getName() + ".len";
		Function *func = module->getFunction(name);

		if (!func) {
			LLVMContext& context = module->getContext();
			func = cast<Function>(module->getOrInsertFunction(name, seqIntLLVM(context)));
			func->setDoesNotThrow();
			func->setLinkage(GlobalValue::PrivateLinkage);
			func->addFnAttr(Attribute::AlwaysInline);
			BasicBlock *block = BasicBlock::Create(context, "entry", func);
			IRBuilder<> builder(block);
			builder.CreateRet(ConstantInt::get(seqIntLLVM(context), this->k));
		}

		return func;
	}), true);

	types::IntNType *iType = types::IntNType::get(2*k, false);
	addMethod("as_int", new BaseFuncLite({this}, iType, [this,iType](Module *module) {
		const std::string name = "seq." + getName() + ".as_int";
		Function *func = module->getFunction(name);

		if (!func) {
			LLVMContext& context = module->getContext();
			func = cast<Function>(module->getOrInsertFunction(name,
			                                                  iType->getLLVMType(context),
			                                                  getLLVMType(context)));
			func->setDoesNotThrow();
			func->setLinkage(GlobalValue::PrivateLinkage);
			func->addFnAttr(Attribute::AlwaysInline);
			Value *arg = func->arg_begin();
			BasicBlock *block = BasicBlock::Create(context, "entry", func);
			IRBuilder<> builder(block);
			builder.CreateRet(builder.CreateBitCast(arg, iType->getLLVMType(context)));
		}

		return func;
	}), true);
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
