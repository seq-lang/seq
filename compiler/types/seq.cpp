#include "lang/seq.h"
#include <iostream>

using namespace seq;
using namespace llvm;

types::BaseSeqType::BaseSeqType(std::string name)
    : Type(std::move(name), BaseType::get(), false, true) {}

Value *types::BaseSeqType::defaultValue(BasicBlock *block) {
  LLVMContext &context = block->getContext();
  Value *ptr = ConstantPointerNull::get(IntegerType::getInt8PtrTy(context));
  Value *len = zeroLLVM(context);
  return make(ptr, len, block);
}

void types::BaseSeqType::initFields() {
  if (!vtable.fields.empty())
    return;

  vtable.fields = {{"len", {0, Int}}, {"ptr", {1, PtrType::get(Byte)}}};
}

bool types::BaseSeqType::isAtomic() const { return false; }

Type *types::BaseSeqType::getLLVMType(LLVMContext &context) const {
  return StructType::get(seqIntLLVM(context), IntegerType::getInt8PtrTy(context));
}

size_t types::BaseSeqType::size(Module *module) const {
  return module->getDataLayout().getTypeAllocSize(getLLVMType(module->getContext()));
}

/* derived Seq type */
types::SeqType::SeqType() : BaseSeqType("seq") {}

Value *types::SeqType::memb(Value *self, const std::string &name, BasicBlock *block) {
  return BaseSeqType::memb(self, name, block);
}

Value *types::SeqType::setMemb(Value *self, const std::string &name, Value *val,
                               BasicBlock *block) {
  return BaseSeqType::setMemb(self, name, val, block);
}

void types::SeqType::initOps() {
  if (!vtable.magic.empty())
    return;

  vtable.magic = {
      {"__new__",
       {PtrType::get(Byte), Int},
       Seq,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return make(args[0], args[1], b.GetInsertBlock());
       },
       true},
  };

  /* TODO: Fix "_kmer_in_seq" builtin to not be generic
  for (unsigned k = 1; k <= KMer::MAX_LEN; k++) {
    vtable.magic.push_back(
        {"__contains__",
         {KMer::get(k)},
         Bool,
         [k](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
           Module *module = b.GetInsertBlock()->getModule();
           Func *f = Func::getBuiltin("_kmer_in_seq")->realize({KMer::get(k)});
           f->codegen(b.GetInsertBlock()->getModule());
           return b.CreateCall(f->getFunc(module), {args[0], self});
         },
         false});
  }
   */
}

Value *types::SeqType::make(Value *ptr, Value *len, BasicBlock *block) {
  LLVMContext &context = ptr->getContext();
  Value *self = UndefValue::get(getLLVMType(context));
  self = setMemb(self, "ptr", ptr, block);
  self = setMemb(self, "len", len, block);
  return self;
}

types::SeqType *types::SeqType::get() noexcept {
  static types::SeqType instance;
  return &instance;
}

/* derived Str type */
types::StrType::StrType() : BaseSeqType("str") {}

Value *types::StrType::memb(Value *self, const std::string &name, BasicBlock *block) {
  return BaseSeqType::memb(self, name, block);
}

Value *types::StrType::setMemb(Value *self, const std::string &name, Value *val,
                               BasicBlock *block) {
  return BaseSeqType::setMemb(self, name, val, block);
}

void types::StrType::initOps() {
  if (!vtable.magic.empty())
    return;

  vtable.magic = {
      {"__new__",
       {PtrType::get(Byte), Int},
       Str,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return make(args[0], args[1], b.GetInsertBlock());
       },
       true},
  };

  addMethod("memcpy",
            new BaseFuncLite(
                {PtrType::get(Byte), PtrType::get(Byte), Int}, Void,
                [](Module *module) {
                  const std::string name = "seq.memcpy";
                  Function *func = module->getFunction(name);

                  if (!func) {
                    LLVMContext &context = module->getContext();
                    func = cast<Function>(module->getOrInsertFunction(
                        name, llvm::Type::getVoidTy(context),
                        IntegerType::getInt8PtrTy(context),
                        IntegerType::getInt8PtrTy(context), seqIntLLVM(context)));
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
                }),
            true);

  addMethod("memmove",
            new BaseFuncLite(
                {PtrType::get(Byte), PtrType::get(Byte), Int}, Void,
                [](Module *module) {
                  const std::string name = "seq.memmove";
                  Function *func = module->getFunction(name);

                  if (!func) {
                    LLVMContext &context = module->getContext();
                    func = cast<Function>(module->getOrInsertFunction(
                        name, llvm::Type::getVoidTy(context),
                        IntegerType::getInt8PtrTy(context),
                        IntegerType::getInt8PtrTy(context), seqIntLLVM(context)));
                    func->setDoesNotThrow();
                    func->setLinkage(GlobalValue::PrivateLinkage);
                    func->addFnAttr(Attribute::AlwaysInline);
                    auto iter = func->arg_begin();
                    Value *dst = iter++;
                    Value *src = iter++;
                    Value *len = iter;
                    BasicBlock *block = BasicBlock::Create(context, "entry", func);
                    makeMemMove(dst, src, len, block);
                    IRBuilder<> builder(block);
                    builder.CreateRetVoid();
                  }

                  return func;
                }),
            true);

  addMethod("memset",
            new BaseFuncLite(
                {PtrType::get(Byte), Byte, Int}, Void,
                [](Module *module) {
                  const std::string name = "seq.memset";
                  Function *func = module->getFunction(name);

                  if (!func) {
                    LLVMContext &context = module->getContext();
                    func = cast<Function>(module->getOrInsertFunction(
                        name, llvm::Type::getVoidTy(context),
                        IntegerType::getInt8PtrTy(context),
                        IntegerType::getInt8Ty(context), seqIntLLVM(context)));
                    func->setDoesNotThrow();
                    func->setLinkage(GlobalValue::PrivateLinkage);
                    func->addFnAttr(Attribute::AlwaysInline);
                    auto iter = func->arg_begin();
                    Value *dst = iter++;
                    Value *val = iter++;
                    Value *len = iter;
                    BasicBlock *block = BasicBlock::Create(context, "entry", func);
                    IRBuilder<> builder(block);
                    builder.CreateMemSet(dst, val, len, 0);
                    builder.CreateRetVoid();
                  }

                  return func;
                }),
            true);
}

Value *types::StrType::make(Value *ptr, Value *len, BasicBlock *block) {
  LLVMContext &context = ptr->getContext();
  Value *self = UndefValue::get(getLLVMType(context));
  self = setMemb(self, "ptr", ptr, block);
  self = setMemb(self, "len", len, block);
  return self;
}

types::StrType *types::StrType::get() noexcept {
  static types::StrType instance;
  return &instance;
}

/*
 * k-mer types (fixed-length short sequences)
 */

types::KMer::KMer(unsigned k) : Type("k-mer", BaseType::get(), false, true), k(k) {
  if (k == 0 || k > MAX_LEN)
    throw exc::SeqException("k-mer length must be between 1 and " +
                            std::to_string(MAX_LEN));
}

unsigned types::KMer::getK() { return k; }

std::string types::KMer::getName() const { return std::to_string(k) + "-mer"; }

Value *types::KMer::defaultValue(BasicBlock *block) {
  return ConstantInt::get(getLLVMType(block->getContext()), 0);
}

// table mapping ASCII characters to 2-bit encodings
static GlobalVariable *get2bitTable(Module *module,
                                    const std::string &name = "seq.2bit_table") {
  LLVMContext &context = module->getContext();
  Type *ty = IntegerType::getIntNTy(context, 2);
  GlobalVariable *table = module->getGlobalVariable(name);

  if (!table) {
    std::vector<Constant *> v(256, ConstantInt::get(ty, 0));
    v['A'] = v['a'] = ConstantInt::get(ty, 0);
    v['C'] = v['c'] = ConstantInt::get(ty, 1);
    v['G'] = v['g'] = ConstantInt::get(ty, 2);
    v['T'] = v['t'] = v['U'] = v['u'] = ConstantInt::get(ty, 3);

    auto *arrTy = llvm::ArrayType::get(IntegerType::getIntNTy(context, 2), v.size());
    table = new GlobalVariable(*module, arrTy, true, GlobalValue::PrivateLinkage,
                               ConstantArray::get(arrTy, v), name);
  }

  return table;
}

// table mapping 2-bit encodings to ASCII characters
static GlobalVariable *get2bitTableInv(Module *module,
                                       const std::string &name = "seq.2bit_table_inv") {
  LLVMContext &context = module->getContext();
  GlobalVariable *table = module->getGlobalVariable(name);

  if (!table) {
    table = new GlobalVariable(
        *module, llvm::ArrayType::get(IntegerType::getInt8Ty(context), 4), true,
        GlobalValue::PrivateLinkage,
        ConstantDataArray::getString(context, "ACGT", false), name);
  }

  return table;
}

static unsigned revcompBits(unsigned n) {
  unsigned c1 = (n & (3u << 0u)) << 6u;
  unsigned c2 = (n & (3u << 2u)) << 2u;
  unsigned c3 = (n & (3u << 4u)) >> 2u;
  unsigned c4 = (n & (3u << 6u)) >> 6u;
  return ~(c1 | c2 | c3 | c4) & 0xffu;
}

// table mapping 8-bit encoded 4-mers to reverse complement encoded 4-mers
static GlobalVariable *getRevCompTable(Module *module,
                                       const std::string &name = "seq.revcomp_table") {
  LLVMContext &context = module->getContext();
  Type *ty = IntegerType::getInt8Ty(context);
  GlobalVariable *table = module->getGlobalVariable(name);

  if (!table) {
    std::vector<Constant *> v(256, ConstantInt::get(ty, 0));
    for (unsigned i = 0; i < v.size(); i++)
      v[i] = ConstantInt::get(ty, revcompBits(i));

    auto *arrTy = llvm::ArrayType::get(IntegerType::getInt8Ty(context), v.size());
    table = new GlobalVariable(*module, arrTy, true, GlobalValue::PrivateLinkage,
                               ConstantArray::get(arrTy, v), name);
  }

  return table;
}

/*
 * Here we create functions for k-mer sliding window shifts. For example, if
 * some k-mer represents a portion of a longer read, we can "shift in" the next
 * base of the sequence via `kmer >> read[i]`, or equivalently in the other
 * direction.
 *
 * dir=false for left; dir=true for right
 */
static Function *getShiftFunc(types::KMer *kmerType, Module *module, bool dir) {
  const std::string name = "seq." + kmerType->getName() + ".sh" + (dir ? "r" : "l");
  LLVMContext &context = module->getContext();
  Function *func = module->getFunction(name);

  if (!func) {
    GlobalVariable *table = get2bitTable(module);

    func = cast<Function>(module->getOrInsertFunction(
        name, kmerType->getLLVMType(context), kmerType->getLLVMType(context),
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
    Value *rc = builder.CreateICmpSLT(len, zeroLLVM(context));
    len = builder.CreateSelect(rc, builder.CreateNeg(len), len);

    BasicBlock *loop = BasicBlock::Create(context, "while", func);
    builder.CreateBr(loop);
    builder.SetInsertPoint(loop);

    PHINode *control = builder.CreatePHI(seqIntLLVM(context), 2);
    PHINode *result = builder.CreatePHI(kmerType->getLLVMType(context), 2);
    control->addIncoming(zeroLLVM(context), entry);
    result->addIncoming(kmer, entry);
    Value *cond = builder.CreateICmpSLT(control, len);

    BasicBlock *body = BasicBlock::Create(context, "body", func);
    BranchInst *branch =
        builder.CreateCondBr(cond, body, body); // we set false-branch below

    builder.SetInsertPoint(body);
    Value *kmerMod = nullptr;

    if (dir) {
      // right slide
      Value *idx = builder.CreateSub(len, oneLLVM(context));
      idx = builder.CreateSub(idx, control);
      // true index if sequence reverse complemented:
      Value *idxBack = builder.CreateSub(builder.CreateSub(len, oneLLVM(context)), idx);
      idx = builder.CreateSelect(rc, idxBack, idx);

      kmerMod = builder.CreateLShr(result, 2);
      Value *base = builder.CreateLoad(builder.CreateGEP(ptr, idx));
      base = builder.CreateZExt(base, builder.getInt64Ty());
      Value *bits = builder.CreateLoad(
          builder.CreateInBoundsGEP(table, {builder.getInt64(0), base}));
      bits = builder.CreateSelect(rc, builder.CreateNot(bits), bits);
      bits = builder.CreateZExt(bits, kmerType->getLLVMType(context));
      bits = builder.CreateShl(bits, 2 * (kmerType->getK() - 1));
      kmerMod = builder.CreateOr(kmerMod, bits);
    } else {
      // left slide
      // true index if sequence reverse complemented:
      Value *idxBack =
          builder.CreateSub(builder.CreateSub(len, oneLLVM(context)), control);
      Value *idx = builder.CreateSelect(rc, idxBack, control);

      kmerMod = builder.CreateShl(result, 2);
      Value *base = builder.CreateLoad(builder.CreateGEP(ptr, idx));
      base = builder.CreateZExt(base, builder.getInt64Ty());
      Value *bits = builder.CreateLoad(
          builder.CreateInBoundsGEP(table, {builder.getInt64(0), base}));
      bits = builder.CreateSelect(rc, builder.CreateNot(bits), bits);
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
static Function *getInitFunc(types::KMer *kmerType, Module *module, bool rc = false) {
  const std::string name = "seq." + kmerType->getName() + ".init" + (rc ? ".rc" : "");
  LLVMContext &context = module->getContext();
  Function *func = module->getFunction(name);

  if (!func) {
    func = cast<Function>(
        rc ? module->getOrInsertFunction(name, kmerType->getLLVMType(context),
                                         types::Seq->getLLVMType(context),
                                         types::Bool->getLLVMType(context))
           : module->getOrInsertFunction(name, kmerType->getLLVMType(context),
                                         types::Seq->getLLVMType(context)));
    func->setDoesNotThrow();
    func->setLinkage(GlobalValue::PrivateLinkage);

    auto iter = func->arg_begin();
    Value *seq = iter++;
    Value *rcVal = rc ? iter : nullptr;
    BasicBlock *block = BasicBlock::Create(context, "entry", func);
    IRBuilder<> builder(block);

    const unsigned k = kmerType->getK();
    GlobalVariable *table = get2bitTable(module);
    Value *ptr = types::Seq->memb(seq, "ptr", block);
    Value *kmer = kmerType->defaultValue(block);

    for (unsigned i = 0; i < k; i++) {
      Value *base = builder.CreateLoad(builder.CreateGEP(ptr, builder.getInt64(i)));
      base = builder.CreateZExt(base, builder.getInt64Ty());
      Value *bits = builder.CreateLoad(
          builder.CreateInBoundsGEP(table, {builder.getInt64(0), base}));
      bits = builder.CreateZExt(bits, kmerType->getLLVMType(context));

      Value *shift = builder.CreateShl(bits, (k - i - 1) * 2);
      kmer = builder.CreateOr(kmer, shift);
    }

    Value *kmerRC = kmerType->callMagic("__invert__", {}, kmer, {}, block, nullptr);
    if (!rcVal) {
      Value *len = types::Seq->memb(seq, "len", block);
      rcVal = builder.CreateICmpSLT(len, zeroLLVM(context));
    } else {
      rcVal = builder.CreateZExtOrTrunc(rcVal, IntegerType::getInt1Ty(context));
    }
    builder.CreateRet(builder.CreateSelect(rcVal, kmerRC, kmer));
  }

  return func;
}

/*
 * KMer-to-Str conversion function
 */
static Function *getStrFunc(types::KMer *kmerType, Module *module) {
  const std::string name = "seq." + kmerType->getName() + ".str";
  LLVMContext &context = module->getContext();
  Function *func = module->getFunction(name);

  if (!func) {
    func = cast<Function>(module->getOrInsertFunction(
        name, types::Str->getLLVMType(context), kmerType->getLLVMType(context)));
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
      unsigned shift = (k - i - 1) * 2;
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

static Value *codegenRevCompByBitShift(types::KMer *kmerType, Value *self,
                                       IRBuilder<> &b) {
  const unsigned k = kmerType->getK();
  LLVMContext &context = b.getContext();

  unsigned kpow2 = 1;
  while (kpow2 < k)
    kpow2 *= 2;
  const unsigned w = 2 * kpow2;

  llvm::Type *ty = IntegerType::get(context, w);
  Value *comp = b.CreateNot(self);
  comp = b.CreateZExt(comp, ty);
  Value *result = comp;

  for (unsigned i = 2; i <= kpow2; i = i * 2) {
    Value *mask = ConstantInt::get(ty, 0);
    Value *bitpattern = ConstantInt::get(ty, 1);
    bitpattern = b.CreateShl(bitpattern, i);
    bitpattern = b.CreateSub(bitpattern, ConstantInt::get(ty, 1));

    unsigned j = 0;
    while (j < w) {
      Value *shift = b.CreateShl(bitpattern, j);
      mask = b.CreateOr(mask, shift);
      j += 2 * i;
    }

    Value *r1 = b.CreateLShr(result, i);
    r1 = b.CreateAnd(r1, mask);
    Value *r2 = b.CreateAnd(result, mask);
    r2 = b.CreateShl(r2, i);
    result = b.CreateOr(r1, r2);
  }

  if (w != 2 * k) {
    assert(w > 2 * k);
    result = b.CreateLShr(result, w - (2 * k));
    result = b.CreateTrunc(result, kmerType->getLLVMType(context));
  }
  return result;
}

static Value *codegenRevCompByLookup(types::KMer *kmerType, Value *self,
                                     IRBuilder<> &b) {
  const unsigned k = kmerType->getK();
  LLVMContext &context = b.getContext();
  Module *module = b.GetInsertBlock()->getModule();
  Value *table = getRevCompTable(module);
  Value *mask = ConstantInt::get(kmerType->getLLVMType(context), 0xffu);
  Value *result = ConstantInt::get(kmerType->getLLVMType(context), 0);

  // deal with 8-bit chunks:
  for (unsigned i = 0; i < k / 4; i++) {
    Value *slice = b.CreateShl(mask, i * 8);
    slice = b.CreateAnd(self, slice);
    slice = b.CreateLShr(slice, i * 8);
    slice = b.CreateZExtOrTrunc(slice, b.getInt64Ty());

    Value *sliceRC = b.CreateInBoundsGEP(table, {b.getInt64(0), slice});
    sliceRC = b.CreateLoad(sliceRC);
    sliceRC = b.CreateZExtOrTrunc(sliceRC, kmerType->getLLVMType(context));
    sliceRC = b.CreateShl(sliceRC, (k - 4 * (i + 1)) * 2);
    result = b.CreateOr(result, sliceRC);
  }

  // deal with remaining high bits:
  unsigned rem = k % 4;
  if (rem > 0) {
    mask = ConstantInt::get(kmerType->getLLVMType(context), (1u << (rem * 2)) - 1);
    Value *slice = b.CreateShl(mask, (k - rem) * 2);
    slice = b.CreateAnd(self, slice);
    slice = b.CreateLShr(slice, (k - rem) * 2);
    slice = b.CreateZExtOrTrunc(slice, b.getInt64Ty());

    Value *sliceRC = b.CreateInBoundsGEP(table, {b.getInt64(0), slice});
    sliceRC = b.CreateLoad(sliceRC);
    sliceRC = b.CreateAShr(sliceRC,
                           (4 - rem) * 2); // slice isn't full 8-bits, so shift out junk
    sliceRC = b.CreateZExtOrTrunc(sliceRC, kmerType->getLLVMType(context));
    sliceRC = b.CreateAnd(sliceRC, mask);
    result = b.CreateOr(result, sliceRC);
  }

  return result;
}

static Value *codegenRevCompBySIMD(types::KMer *kmerType, Value *self, IRBuilder<> &b) {
  const unsigned k = kmerType->getK();
  LLVMContext &context = b.getContext();
  Value *comp = b.CreateNot(self);

  llvm::Type *ty = kmerType->getLLVMType(context);
  const unsigned w = ((2 * k + 7) / 8) * 8;
  const unsigned m = w / 8;

  if (w != 2 * k) {
    ty = IntegerType::get(context, w);
    comp = b.CreateZExt(comp, ty);
  }

  VectorType *vecTy = VectorType::get(b.getInt8Ty(), m);
  std::vector<unsigned> shufMask;
  for (unsigned i = 0; i < m; i++)
    shufMask.push_back(m - 1 - i);

  Value *vec = UndefValue::get(VectorType::get(ty, 1));
  vec = b.CreateInsertElement(vec, comp, (uint64_t)0);
  vec = b.CreateBitCast(vec, vecTy);
  // shuffle reverses bytes
  vec = b.CreateShuffleVector(vec, UndefValue::get(vecTy), shufMask);

  // shifts reverse 2-bit chunks in each byte
  Value *shift1 = ConstantVector::getSplat(m, b.getInt8(6));
  Value *shift2 = ConstantVector::getSplat(m, b.getInt8(2));
  Value *mask1 = ConstantVector::getSplat(m, b.getInt8(0x0c));
  Value *mask2 = ConstantVector::getSplat(m, b.getInt8(0x30));

  Value *vec1 = b.CreateLShr(vec, shift1);
  Value *vec2 = b.CreateShl(vec, shift1);
  Value *vec3 = b.CreateLShr(vec, shift2);
  Value *vec4 = b.CreateShl(vec, shift2);
  vec3 = b.CreateAnd(vec3, mask1);
  vec4 = b.CreateAnd(vec4, mask2);

  vec = b.CreateOr(vec1, vec2);
  vec = b.CreateOr(vec, vec3);
  vec = b.CreateOr(vec, vec4);

  vec = b.CreateBitCast(vec, VectorType::get(ty, 1));
  Value *result = b.CreateExtractElement(vec, (uint64_t)0);
  if (w != 2 * k) {
    assert(w > 2 * k);
    result = b.CreateLShr(result, w - (2 * k));
    result = b.CreateTrunc(result, kmerType->getLLVMType(context));
  }
  return result;
}

void types::KMer::initOps() {
  if (!vtable.magic.empty())
    return;

  types::IntNType *iType = types::IntNType::get(2 * k, false);
  vtable.magic = {
      {"__init__",
       {},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return ConstantInt::get(getLLVMType(b.getContext()), 0);
       },
       false},

      {"__init__",
       {this},
       this,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) { return args[0]; },
       false},

      {"__init__",
       {Seq},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         Function *initFunc = getInitFunc(this, b.GetInsertBlock()->getModule());
         return b.CreateCall(initFunc, args[0]);
       },
       false},

      {"__init__",
       {Seq, Bool},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         Function *initFunc = getInitFunc(this, b.GetInsertBlock()->getModule(), true);
         return b.CreateCall(initFunc, {args[0], args[1]});
       },
       false},

      {"__init__",
       {IntNType::get(2 * k, false)},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateBitCast(args[0], getLLVMType(b.getContext()));
       },
       false},

      {"__init__",
       {Int},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExtOrTrunc(args[0], getLLVMType(b.getContext()));
       },
       false},

      {"__str__",
       {},
       Str,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         Function *strFunc = getStrFunc(this, b.GetInsertBlock()->getModule());
         return b.CreateCall(strFunc, self);
       },
       false},

      {"__copy__",
       {},
       this,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) { return self; },
       false},

      {"__getitem__",
       {Int},
       KMer::get(1),
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         LLVMContext &context = b.getContext();
         llvm::Type *type = getLLVMType(context);
         Value *mask = ConstantInt::get(type, 3);
         Value *backIdx =
             b.CreateAdd(args[0], ConstantInt::get(seqIntLLVM(context), k));
         Value *negIdx = b.CreateICmpSLT(args[0], zeroLLVM(context));
         Value *idx = b.CreateSelect(negIdx, backIdx, args[0]);
         Value *shift = b.CreateSub(ConstantInt::get(seqIntLLVM(context), k - 1), idx);
         shift = b.CreateShl(shift, 1); // 2 bits per base
         shift = b.CreateZExtOrTrunc(shift, type);

         mask = b.CreateShl(mask, shift);
         Value *result = b.CreateAnd(self, mask);
         result = b.CreateAShr(result, shift);
         return b.CreateZExtOrTrunc(result, KMer::get(1)->getLLVMType(context));
       },
       false},

      // reverse complement
      {"__invert__",
       {},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         // The following are heuristics found to be roughly optimal on
         // several architectures. For smaller k, lookup is almost always
         // better. For larger k, SIMD is almost always better. For medium k,
         // it varies based on whether k is a power of 2, but bitwise is
         // almost always close to (if not) the best.
         if (k <= 20) {
           return codegenRevCompByLookup(this, self, b);
         } else if (k < 32) {
           return codegenRevCompByBitShift(this, self, b);
         } else {
           return codegenRevCompBySIMD(this, self, b);
         }
       },
       false},

      {"__rc_bitwise__",
       {},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return codegenRevCompByBitShift(this, self, b);
       },
       false},

      {"__rc_lookup__",
       {},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return codegenRevCompByLookup(this, self, b);
       },
       false},

      {"__rc_simd__",
       {},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return codegenRevCompBySIMD(this, self, b);
       },
       false},

      // slide window left
      {"__lshift__",
       {Seq},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         Module *module = b.GetInsertBlock()->getModule();
         return b.CreateCall(getShiftFunc(this, module, false), {self, args[0]});
       },
       false},

      // slide window right
      {"__rshift__",
       {Seq},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         Module *module = b.GetInsertBlock()->getModule();
         return b.CreateCall(getShiftFunc(this, module, true), {self, args[0]});
       },
       false},

      // Hamming distance
      {"__sub__",
       {this},
       Int,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
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
         LLVMContext &context = b.getContext();
         Value *mask1 = ConstantInt::get(getLLVMType(context), 0);
         Value *one = ConstantInt::get(getLLVMType(context), 1);
         for (unsigned i = 0; i < getK(); i++) {
           Value *shift = b.CreateShl(one, 2 * i);
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

         Function *popcnt = Intrinsic::getDeclaration(
             b.GetInsertBlock()->getModule(), Intrinsic::ctpop, {getLLVMType(context)});
         Value *result = b.CreateCall(popcnt, diff);
         result = b.CreateZExtOrTrunc(result, seqIntLLVM(context));
         Value *resultNeg = b.CreateNeg(result);
         Value *order = b.CreateICmpUGE(self, args[0]);
         return b.CreateSelect(order, result, resultNeg);
       },
       false},

      {"__hash__",
       {},
       Int,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         Value *hash = b.CreateZExtOrTrunc(self, seqIntLLVM(b.getContext()));
         if (getK() > 32) {
           // make sure bases on both ends are involved in hash:
           Value *aux = b.CreateLShr(self, 2 * getK() - 64);
           aux = b.CreateZExtOrTrunc(aux, seqIntLLVM(b.getContext()));
           hash = b.CreateXor(hash, aux);
         }
         return hash;
       },
       false},

      {"__len__",
       {},
       Int,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return ConstantInt::get(seqIntLLVM(b.getContext()), k, true);
       },
       false},

      {"__eq__",
       {this},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateICmpEQ(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__ne__",
       {this},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateICmpNE(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__lt__",
       {this},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateICmpULT(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__gt__",
       {this},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateICmpUGT(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__le__",
       {this},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateICmpULE(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__ge__",
       {this},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateICmpUGE(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__contains__",
       {Seq},
       Bool,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         BasicBlock *block = b.GetInsertBlock();
         Value *s1 = this->strValue(self, block, nullptr);
         Value *s2 = Seq->strValue(args[0], block, nullptr);
         return Str->callMagic("__contains__", {Str}, s1, {s2}, block, nullptr);
       },
       false},

      {"__pickle__",
       {PtrType::get(Byte)},
       Void,
       [iType](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         BasicBlock *block = b.GetInsertBlock();
         iType->callMagic("__pickle__", {PtrType::get(Byte)}, self, {args[0]}, block,
                          nullptr);
         return (Value *)nullptr;
       },
       false},

      {"__unpickle__",
       {PtrType::get(Byte)},
       this,
       [iType](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         BasicBlock *block = b.GetInsertBlock();
         return iType->callMagic("__unpickle__", {PtrType::get(Byte)}, nullptr,
                                 {args[0]}, block, nullptr);
       },
       true},
  };

  if (k == 1) {
    vtable.magic.push_back({"__init__",
                            {Byte},
                            this,
                            [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
                              GlobalVariable *table =
                                  get2bitTable(b.GetInsertBlock()->getModule());
                              Value *base = b.CreateZExt(args[0], b.getInt64Ty());
                              Value *bits = b.CreateLoad(
                                  b.CreateInBoundsGEP(table, {b.getInt64(0), base}));
                              return bits;
                            },
                            false});
  }

  addMethod("len",
            new BaseFuncLite(
                {}, types::IntType::get(),
                [this](Module *module) {
                  const std::string name = "seq." + getName() + ".len";
                  Function *func = module->getFunction(name);

                  if (!func) {
                    LLVMContext &context = module->getContext();
                    func = cast<Function>(
                        module->getOrInsertFunction(name, seqIntLLVM(context)));
                    func->setDoesNotThrow();
                    func->setLinkage(GlobalValue::PrivateLinkage);
                    func->addFnAttr(Attribute::AlwaysInline);
                    BasicBlock *block = BasicBlock::Create(context, "entry", func);
                    IRBuilder<> builder(block);
                    builder.CreateRet(ConstantInt::get(seqIntLLVM(context), this->k));
                  }

                  return func;
                }),
            true);

  addMethod("as_int",
            new BaseFuncLite(
                {this}, iType,
                [this, iType](Module *module) {
                  const std::string name = "seq." + getName() + ".as_int";
                  Function *func = module->getFunction(name);

                  if (!func) {
                    LLVMContext &context = module->getContext();
                    func = cast<Function>(module->getOrInsertFunction(
                        name, iType->getLLVMType(context), getLLVMType(context)));
                    func->setDoesNotThrow();
                    func->setLinkage(GlobalValue::PrivateLinkage);
                    func->addFnAttr(Attribute::AlwaysInline);
                    Value *arg = func->arg_begin();
                    BasicBlock *block = BasicBlock::Create(context, "entry", func);
                    IRBuilder<> builder(block);
                    builder.CreateRet(
                        builder.CreateBitCast(arg, iType->getLLVMType(context)));
                  }

                  return func;
                }),
            true);
}

bool types::KMer::isAtomic() const { return true; }

bool types::KMer::is(seq::types::Type *type) const {
  types::KMer *kmer = type->asKMer();
  return kmer && k == kmer->k;
}

Type *types::KMer::getLLVMType(LLVMContext &context) const {
  return IntegerType::getIntNTy(context, 2 * k);
}

size_t types::KMer::size(Module *module) const {
  return module->getDataLayout().getTypeAllocSize(getLLVMType(module->getContext()));
}

types::KMer *types::KMer::asKMer() { return this; }

types::KMer *types::KMer::get(unsigned k) {
  static std::map<unsigned, KMer *> cache;

  if (cache.find(k) != cache.end())
    return cache.find(k)->second;

  auto *kmerType = new KMer(k);
  cache.insert({k, kmerType});
  return kmerType;
}
