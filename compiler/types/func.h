#pragma once

#include "types/record.h"
#include "types/types.h"

namespace seq {
class Expr;
namespace types {

class FuncType : public Type {
private:
  std::vector<Type *> inTypes;
  Type *outType;
  FuncType(std::vector<types::Type *> inTypes, Type *outType);

public:
  FuncType(FuncType const &) = delete;
  void operator=(FuncType const &) = delete;

  unsigned argCount() const;

  llvm::Value *call(BaseFunc *base, llvm::Value *self,
                    const std::vector<llvm::Value *> &args, llvm::BasicBlock *block,
                    llvm::BasicBlock *normal, llvm::BasicBlock *unwind) override;

  llvm::Value *defaultValue(llvm::BasicBlock *block) override;

  void initOps() override;
  bool is(Type *type) const override;
  unsigned numBaseTypes() const override;
  Type *getBaseType(unsigned idx) const override;
  Type *getCallType(const std::vector<Type *> &inTypes) override;
  llvm::Type *getLLVMType(llvm::LLVMContext &context) const override;
  size_t size(llvm::Module *module) const override;
  static FuncType *get(std::vector<Type *> inTypes, Type *outType);
};

// Generator types really represent generator handles in LLVM
class GenType : public Type {
public:
  enum GenTypeKind { NORMAL, PREFETCH, INTERALIGN };
  struct InterAlignParams { // see bio/align.seq for definition
    Expr *a, *b, *ambig, *gapo, *gape, *score_only, *bandwidth, *zdrop, *end_bonus;
    InterAlignParams()
        : a(nullptr), b(nullptr), ambig(nullptr), gapo(nullptr), gape(nullptr),
          score_only(nullptr), bandwidth(nullptr), zdrop(nullptr), end_bonus(nullptr) {}
  };

private:
  Type *outType;
  GenTypeKind kind;
  InterAlignParams alnParams;
  explicit GenType(Type *outType, GenTypeKind = GenTypeKind::NORMAL);

public:
  GenType(GenType const &) = delete;
  void operator=(FuncType const &) = delete;

  bool isAtomic() const override;

  llvm::Value *defaultValue(llvm::BasicBlock *block) override;
  llvm::Value *done(llvm::Value *self, llvm::BasicBlock *block);
  void resume(llvm::Value *self, llvm::BasicBlock *block, llvm::BasicBlock *normal,
              llvm::BasicBlock *unwind);
  llvm::Value *promise(llvm::Value *self, llvm::BasicBlock *block,
                       bool returnPtr = false);
  void send(llvm::Value *self, llvm::Value *val, llvm::BasicBlock *block);
  void destroy(llvm::Value *self, llvm::BasicBlock *block);
  bool fromPrefetch();
  bool fromInterAlign();
  void setAlignParams(InterAlignParams alnParams);
  InterAlignParams getAlignParams();

  void initOps() override;
  bool is(Type *type) const override;
  unsigned numBaseTypes() const override;
  Type *getBaseType(unsigned idx) const override;
  llvm::Type *getLLVMType(llvm::LLVMContext &context) const override;
  size_t size(llvm::Module *module) const override;

  GenType *asGen() override;

  static GenType *get(Type *outType, GenTypeKind kind = GenTypeKind::NORMAL) noexcept;
  static GenType *get(GenTypeKind kind = GenTypeKind::NORMAL) noexcept;
};

class PartialFuncType : public Type {
private:
  Type *callee;
  std::vector<Type *> callTypes;
  RecordType *contents;

  // callTypes[i] == null means the argument is unknown
  PartialFuncType(Type *callee, std::vector<types::Type *> callTypes);

public:
  PartialFuncType(PartialFuncType const &) = delete;
  void operator=(PartialFuncType const &) = delete;

  std::vector<Type *> getCallTypes() const;

  bool isAtomic() const override;

  llvm::Value *call(BaseFunc *base, llvm::Value *self,
                    const std::vector<llvm::Value *> &args, llvm::BasicBlock *block,
                    llvm::BasicBlock *normal, llvm::BasicBlock *unwind) override;

  llvm::Value *defaultValue(llvm::BasicBlock *block) override;

  bool is(Type *type) const override;
  unsigned numBaseTypes() const override;
  Type *getBaseType(unsigned idx) const override;
  Type *getCallType(const std::vector<Type *> &inTypes) override;
  llvm::Type *getLLVMType(llvm::LLVMContext &context) const override;
  size_t size(llvm::Module *module) const override;
  static PartialFuncType *get(Type *callee, std::vector<types::Type *> callTypes);

  llvm::Value *make(llvm::Value *func, std::vector<llvm::Value *> args,
                    llvm::BasicBlock *block);
};

} // namespace types
} // namespace seq
