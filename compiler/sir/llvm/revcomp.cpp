#include "revcomp.h"

namespace seq {
namespace ir {

namespace {
unsigned revcompBits(unsigned n) {
  unsigned c1 = (n & (3u << 0u)) << 6u;
  unsigned c2 = (n & (3u << 2u)) << 2u;
  unsigned c3 = (n & (3u << 4u)) >> 2u;
  unsigned c4 = (n & (3u << 6u)) >> 6u;
  return ~(c1 | c2 | c3 | c4) & 0xffu;
}
} // namespace

llvm::GlobalVariable *getRevCompTable(llvm::Module *module, const std::string &name) {
  llvm::LLVMContext &context = module->getContext();
  llvm::Type *ty = llvm::Type::getInt8Ty(context);
  llvm::GlobalVariable *table = module->getGlobalVariable(name);

  if (!table) {
    std::vector<llvm::Constant *> v(256, llvm::ConstantInt::get(ty, 0));
    for (unsigned i = 0; i < v.size(); i++)
      v[i] = llvm::ConstantInt::get(ty, revcompBits(i));

    auto *arrTy = llvm::ArrayType::get(llvm::Type::getInt8Ty(context), v.size());
    table = new llvm::GlobalVariable(*module, arrTy, true,
                                     llvm::GlobalValue::PrivateLinkage,
                                     llvm::ConstantArray::get(arrTy, v), name);
  }

  return table;
}

llvm::Value *codegenRevCompByBitShift(const unsigned k, llvm::Value *self,
                                      llvm::IRBuilder<> &builder) {
  llvm::Type *kmerType = builder.getIntNTy(2 * k);
  llvm::LLVMContext &context = builder.getContext();

  unsigned kpow2 = 1;
  while (kpow2 < k)
    kpow2 *= 2;
  const unsigned w = 2 * kpow2;

  llvm::Type *ty = llvm::IntegerType::get(context, w);
  llvm::Value *comp = builder.CreateNot(self);
  comp = builder.CreateZExt(comp, ty);
  llvm::Value *result = comp;

  for (unsigned i = 2; i <= kpow2; i = i * 2) {
    llvm::Value *mask = llvm::ConstantInt::get(ty, 0);
    llvm::Value *bitpattern = llvm::ConstantInt::get(ty, 1);
    bitpattern = builder.CreateShl(bitpattern, i);
    bitpattern = builder.CreateSub(bitpattern, llvm::ConstantInt::get(ty, 1));

    unsigned j = 0;
    while (j < w) {
      llvm::Value *shift = builder.CreateShl(bitpattern, j);
      mask = builder.CreateOr(mask, shift);
      j += 2 * i;
    }

    llvm::Value *r1 = builder.CreateLShr(result, i);
    r1 = builder.CreateAnd(r1, mask);
    llvm::Value *r2 = builder.CreateAnd(result, mask);
    r2 = builder.CreateShl(r2, i);
    result = builder.CreateOr(r1, r2);
  }

  if (w != 2 * k) {
    assert(w > 2 * k);
    result = builder.CreateLShr(result, w - (2 * k));
    result = builder.CreateTrunc(result, kmerType);
  }
  return result;
}

llvm::Value *codegenRevCompByLookup(const unsigned k, llvm::Value *self,
                                    llvm::IRBuilder<> &builder) {
  llvm::Type *kmerType = builder.getIntNTy(2 * k);
  llvm::Module *module = builder.GetInsertBlock()->getModule();
  llvm::Value *table = getRevCompTable(module);
  llvm::Value *mask = llvm::ConstantInt::get(kmerType, 0xffu);
  llvm::Value *result = llvm::ConstantInt::get(kmerType, 0);

  // deal with 8-bit chunks:
  for (unsigned i = 0; i < k / 4; i++) {
    llvm::Value *slice = builder.CreateShl(mask, i * 8);
    slice = builder.CreateAnd(self, slice);
    slice = builder.CreateLShr(slice, i * 8);
    slice = builder.CreateZExtOrTrunc(slice, builder.getInt64Ty());

    llvm::Value *sliceRC =
        builder.CreateInBoundsGEP(table, {builder.getInt64(0), slice});
    sliceRC = builder.CreateLoad(sliceRC);
    sliceRC = builder.CreateZExtOrTrunc(sliceRC, kmerType);
    sliceRC = builder.CreateShl(sliceRC, (k - 4 * (i + 1)) * 2);
    result = builder.CreateOr(result, sliceRC);
  }

  // deal with remaining high bits:
  unsigned rem = k % 4;
  if (rem > 0) {
    mask = llvm::ConstantInt::get(kmerType, (1u << (rem * 2)) - 1);
    llvm::Value *slice = builder.CreateShl(mask, (k - rem) * 2);
    slice = builder.CreateAnd(self, slice);
    slice = builder.CreateLShr(slice, (k - rem) * 2);
    slice = builder.CreateZExtOrTrunc(slice, builder.getInt64Ty());

    llvm::Value *sliceRC =
        builder.CreateInBoundsGEP(table, {builder.getInt64(0), slice});
    sliceRC = builder.CreateLoad(sliceRC);
    sliceRC =
        builder.CreateAShr(sliceRC,
                           (4 - rem) * 2); // slice isn't full 8-bits, so shift out junk
    sliceRC = builder.CreateZExtOrTrunc(sliceRC, kmerType);
    sliceRC = builder.CreateAnd(sliceRC, mask);
    result = builder.CreateOr(result, sliceRC);
  }

  return result;
}

llvm::Value *codegenRevCompBySIMD(const unsigned k, llvm::Value *self,
                                  llvm::IRBuilder<> &builder) {
  llvm::Type *kmerType = builder.getIntNTy(2 * k);
  llvm::LLVMContext &context = builder.getContext();
  llvm::Value *comp = builder.CreateNot(self);

  llvm::Type *ty = kmerType;
  const unsigned w = ((2 * k + 7) / 8) * 8;
  const unsigned m = w / 8;

  if (w != 2 * k) {
    ty = llvm::IntegerType::get(context, w);
    comp = builder.CreateZExt(comp, ty);
  }

  auto *vecTy = llvm::VectorType::get(builder.getInt8Ty(), m);
  std::vector<unsigned> shufMask;
  for (unsigned i = 0; i < m; i++)
    shufMask.push_back(m - 1 - i);

  llvm::Value *vec = llvm::UndefValue::get(llvm::VectorType::get(ty, 1));
  vec = builder.CreateInsertElement(vec, comp, (uint64_t)0);
  vec = builder.CreateBitCast(vec, vecTy);
  // shuffle reverses bytes
  vec = builder.CreateShuffleVector(vec, llvm::UndefValue::get(vecTy), shufMask);

  // shifts reverse 2-bit chunks in each byte
  llvm::Value *shift1 = llvm::ConstantVector::getSplat(m, builder.getInt8(6));
  llvm::Value *shift2 = llvm::ConstantVector::getSplat(m, builder.getInt8(2));
  llvm::Value *mask1 = llvm::ConstantVector::getSplat(m, builder.getInt8(0x0c));
  llvm::Value *mask2 = llvm::ConstantVector::getSplat(m, builder.getInt8(0x30));

  llvm::Value *vec1 = builder.CreateLShr(vec, shift1);
  llvm::Value *vec2 = builder.CreateShl(vec, shift1);
  llvm::Value *vec3 = builder.CreateLShr(vec, shift2);
  llvm::Value *vec4 = builder.CreateShl(vec, shift2);
  vec3 = builder.CreateAnd(vec3, mask1);
  vec4 = builder.CreateAnd(vec4, mask2);

  vec = builder.CreateOr(vec1, vec2);
  vec = builder.CreateOr(vec, vec3);
  vec = builder.CreateOr(vec, vec4);

  vec = builder.CreateBitCast(vec, llvm::VectorType::get(ty, 1));
  llvm::Value *result = builder.CreateExtractElement(vec, (uint64_t)0);
  if (w != 2 * k) {
    assert(w > 2 * k);
    result = builder.CreateLShr(result, w - (2 * k));
    result = builder.CreateTrunc(result, kmerType);
  }
  return result;
}

llvm::Value *codegenRevCompHeuristic(const unsigned k, llvm::Value *self,
                                     llvm::IRBuilder<> &builder) {
  if (k == 1) {
    return builder.CreateNot(self);
  } else if (k <= 20) {
    return codegenRevCompByLookup(k, self, builder);
  } else if (k < 32) {
    return codegenRevCompByBitShift(k, self, builder);
  } else {
    return codegenRevCompBySIMD(k, self, builder);
  }
}

} // namespace ir
} // namespace seq
