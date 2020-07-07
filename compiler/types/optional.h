#pragma once

#include "types/types.h"

namespace seq {
namespace types {
class OptionalType : public Type {
private:
  Type *baseType;
  explicit OptionalType(Type *baseType);

  bool isRefOpt() const;

public:
  OptionalType(OptionalType const &) = delete;
  void operator=(OptionalType const &) = delete;

  llvm::Value *defaultValue(llvm::BasicBlock *block) override;

  void initFields() override;
  void initOps() override;
  bool isAtomic() const override;
  bool is(Type *type) const override;
  unsigned numBaseTypes() const override;
  Type *getBaseType(unsigned idx) const override;
  llvm::Type *getLLVMType(llvm::LLVMContext &context) const override;
  size_t size(llvm::Module *module) const override;

  OptionalType *asOpt() override;

  llvm::Value *make(llvm::Value *val, llvm::BasicBlock *block);
  llvm::Value *has(llvm::Value *self, llvm::BasicBlock *block);
  llvm::Value *val(llvm::Value *self, llvm::BasicBlock *block);
  static OptionalType *get(Type *baseType) noexcept;
  static OptionalType *get();
};

} // namespace types
} // namespace seq
