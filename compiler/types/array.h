#pragma once

#include "types/types.h"

namespace seq {
namespace types {
class ArrayType : public Type {
private:
  Type *baseType;
  explicit ArrayType(Type *baseType);

public:
  ArrayType(ArrayType const &) = delete;
  void operator=(ArrayType const &) = delete;

  llvm::Value *defaultValue(llvm::BasicBlock *block) override;

  void initOps() override;
  void initFields() override;
  bool isAtomic() const override;
  bool is(Type *type) const override;
  unsigned numBaseTypes() const override;
  Type *getBaseType(unsigned idx) const override;
  llvm::Type *getLLVMType(llvm::LLVMContext &context) const override;
  size_t size(llvm::Module *module) const override;
  llvm::Value *make(llvm::Value *ptr, llvm::Value *len,
                    llvm::BasicBlock *block);
  static ArrayType *get(Type *baseType) noexcept;
  static ArrayType *get() noexcept;
};

} // namespace types
} // namespace seq
