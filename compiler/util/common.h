#pragma once

#include "runtime/lib.h"
#include "util/llvm.h"
#include <cstdint>
#include <memory>
#include <ostream>
#include <stdexcept>

namespace seq {
struct SrcInfo {
  std::string file;
  int line, endLine;
  int col, endCol;
  int id; /// used to differentiate different
  SrcInfo(std::string file, int line, int endLine, int col, int endCol)
      : file(std::move(file)), line(line), endLine(endLine), col(col), endCol(endCol) {
    static int _id(0);
    id = _id++;
  };
  SrcInfo() : SrcInfo("<internal>", 0, 0, 0, 0){};
  friend std::ostream &operator<<(std::ostream &out, const seq::SrcInfo &c);
  bool operator==(const SrcInfo &src) const {
    return /*(file == src.file) && (line == src.line) && (col == src.col) &&*/ (id ==
                                                                                src.id);
  }
};
} // namespace seq

namespace seq {
struct SrcObject {
private:
  SrcInfo info;

public:
  SrcObject() : info() {}
  SrcObject(const SrcObject &s) { setSrcInfo(s.getSrcInfo()); }

  virtual ~SrcObject() = default;

  SrcInfo getSrcInfo() const { return info; }

  void setSrcInfo(SrcInfo info) { this->info = std::move(info); }
};

inline llvm::IntegerType *seqIntLLVM(llvm::LLVMContext &context) {
  return llvm::IntegerType::getIntNTy(context, 8 * sizeof(seq_int_t));
}

inline llvm::Constant *nullPtrLLVM(llvm::LLVMContext &context) {
  return llvm::ConstantPointerNull::get(llvm::PointerType::getInt8PtrTy(context));
}

inline llvm::Constant *zeroLLVM(llvm::LLVMContext &context) {
  return llvm::ConstantInt::get(seqIntLLVM(context), 0);
}

inline llvm::Constant *oneLLVM(llvm::LLVMContext &context) {
  return llvm::ConstantInt::get(seqIntLLVM(context), 1);
}

inline llvm::Value *makeAlloca(llvm::Type *type, llvm::BasicBlock *block,
                               uint64_t n = 1) {
  llvm::LLVMContext &context = block->getContext();
  llvm::IRBuilder<> builder(block);
  llvm::Value *ptr =
      builder.CreateAlloca(type, llvm::ConstantInt::get(seqIntLLVM(context), n));
  return ptr;
}

inline llvm::Value *makeAlloca(llvm::Value *value, llvm::BasicBlock *block,
                               uint64_t n = 1) {
  llvm::IRBuilder<> builder(block);
  llvm::Value *ptr = makeAlloca(value->getType(), block, n);
  builder.CreateStore(value, ptr);
  return ptr;
}

inline void makeMemCpy(llvm::Value *dst, llvm::Value *src, llvm::Value *size,
                       llvm::BasicBlock *block, unsigned align = 0) {
  llvm::IRBuilder<> builder(block);
#if LLVM_VERSION_MAJOR >= 7
  builder.CreateMemCpy(dst, align, src, align, size);
#else
  builder.CreateMemCpy(dst, src, size, align);
#endif
}

inline void makeMemMove(llvm::Value *dst, llvm::Value *src, llvm::Value *size,
                        llvm::BasicBlock *block, unsigned align = 0) {
  llvm::IRBuilder<> builder(block);
#if LLVM_VERSION_MAJOR >= 7
  builder.CreateMemMove(dst, align, src, align, size);
#else
  builder.CreateMemMove(dst, src, size, align);
#endif
}

// Our custom GC allocators:
inline llvm::Function *makeAllocFunc(llvm::Module *module, bool atomic) {
  llvm::LLVMContext &context = module->getContext();
  auto *f = llvm::cast<llvm::Function>(module->getOrInsertFunction(
      atomic ? "seq_alloc_atomic" : "seq_alloc",
      llvm::IntegerType::getInt8PtrTy(context), seqIntLLVM(context)));
  f->setDoesNotThrow();
  f->setReturnDoesNotAlias();
  f->setOnlyAccessesInaccessibleMemory();
  return f;
}

// Standard malloc:
inline llvm::Function *makeMallocFunc(llvm::Module *module) {
  llvm::LLVMContext &context = module->getContext();
  auto *f = llvm::cast<llvm::Function>(module->getOrInsertFunction(
      "malloc", llvm::IntegerType::getInt8PtrTy(context), seqIntLLVM(context)));
  f->setDoesNotThrow();
  f->setReturnDoesNotAlias();
  f->setOnlyAccessesInaccessibleMemory();
  return f;
}

// Standard free:
inline llvm::Function *makeFreeFunc(llvm::Module *module) {
  llvm::LLVMContext &context = module->getContext();
  auto *f = llvm::cast<llvm::Function>(
      module->getOrInsertFunction("free", llvm::Type::getVoidTy(context),
                                  llvm::IntegerType::getInt8PtrTy(context)));
  f->setDoesNotThrow();
  return f;
}

inline llvm::Function *makePersonalityFunc(llvm::Module *module) {
  llvm::LLVMContext &context = module->getContext();
  return llvm::cast<llvm::Function>(module->getOrInsertFunction(
      "seq_personality", llvm::IntegerType::getInt32Ty(context),
      llvm::IntegerType::getInt32Ty(context), llvm::IntegerType::getInt32Ty(context),
      llvm::IntegerType::getInt64Ty(context), llvm::IntegerType::getInt8PtrTy(context),
      llvm::IntegerType::getInt8PtrTy(context)));
}

inline llvm::Function *makeExcAllocFunc(llvm::Module *module) {
  llvm::LLVMContext &context = module->getContext();
  auto *f = llvm::cast<llvm::Function>(module->getOrInsertFunction(
      "seq_alloc_exc", llvm::IntegerType::getInt8PtrTy(context),
      llvm::IntegerType::getInt32Ty(context),
      llvm::IntegerType::getInt8PtrTy(context)));
  f->setDoesNotThrow();
  return f;
}

inline llvm::Function *makeThrowFunc(llvm::Module *module) {
  llvm::LLVMContext &context = module->getContext();
  auto *f = llvm::cast<llvm::Function>(
      module->getOrInsertFunction("seq_throw", llvm::Type::getVoidTy(context),
                                  llvm::IntegerType::getInt8PtrTy(context)));
  f->setDoesNotReturn();
  return f;
}

inline llvm::Function *makeTerminateFunc(llvm::Module *module) {
  llvm::LLVMContext &context = module->getContext();
  auto *f = llvm::cast<llvm::Function>(
      module->getOrInsertFunction("seq_terminate", llvm::Type::getVoidTy(context),
                                  llvm::IntegerType::getInt8PtrTy(context)));
  f->setDoesNotReturn();
  return f;
}

namespace exc {
class SeqException : public SrcObject, public std::runtime_error {
public:
  SeqException(const std::string &msg, SrcInfo info) noexcept
      : SrcObject(), std::runtime_error(msg) {
    setSrcInfo(std::move(info));
  }

  explicit SeqException(const std::string &msg) noexcept : SeqException(msg, {}) {}

  SeqException(const SeqException &e) noexcept
      : SrcObject(e), std::runtime_error(e) // NOLINT
  {}
};
} // namespace exc

} // namespace seq

#define SEQ_RETURN_CLONE(e)                                                            \
  do {                                                                                 \
    auto *__x = (e);                                                                   \
    __x->setSrcInfo(getSrcInfo());                                                     \
    if (getTryCatch())                                                                 \
      __x->setTryCatch(getTryCatch()->clone(ref));                                     \
    return __x;                                                                        \
  } while (0)

#if defined(TAPIR_VERSION_MAJOR)
#define SEQ_HAS_TAPIR 1
#else
#define SEQ_HAS_TAPIR 0
#endif
