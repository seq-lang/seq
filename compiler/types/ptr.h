#pragma once

#include "types/types.h"

namespace seq {
namespace types {
class PtrType : public Type {
private:
  Type *baseType;
  explicit PtrType(Type *baseType);

public:
  PtrType(PtrType const &) = delete;
  void operator=(PtrType const &) = delete;

  llvm::Value *defaultValue(llvm::BasicBlock *block) override;

  void initOps() override;
  bool isAtomic() const override;
  bool is(Type *type) const override;
  unsigned numBaseTypes() const override;
  Type *getBaseType(unsigned idx) const override;
  llvm::Type *getLLVMType(llvm::LLVMContext &context) const override;
  size_t size(llvm::Module *module) const override;
  static PtrType *get(Type *baseType) noexcept;
  static PtrType *get() noexcept;
};

} // namespace types
} // namespace seq
