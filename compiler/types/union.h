#pragma once

#include "types/types.h"
#include <vector>

namespace seq {
namespace types {

class UnionType : public Type {
private:
  std::vector<types::Type *> types;
  explicit UnionType(std::vector<types::Type *> types);
  std::vector<types::Type *> sortedTypes() const;
  llvm::IntegerType *selectorType(llvm::LLVMContext &context) const;
  llvm::Value *interpretAs(llvm::Value *data, llvm::Type *type,
                           llvm::BasicBlock *block) const;

public:
  UnionType(UnionType const &) = delete;
  void operator=(UnionType const &) = delete;

  llvm::Value *defaultValue(llvm::BasicBlock *block) override;

  void initFields() override;
  void initOps() override;
  bool isAtomic() const override;
  bool is(Type *type) const override;
  unsigned numBaseTypes() const override;
  Type *getBaseType(unsigned idx) const override;
  llvm::StructType *getLLVMType(llvm::LLVMContext &context) const override;
  size_t size(llvm::Module *module) const override;

  UnionType *asUnion() override;

  unsigned indexFor(types::Type *type) const;
  llvm::Value *make(types::Type *type, llvm::Value *val, llvm::BasicBlock *block);
  llvm::Value *has(llvm::Value *self, types::Type *type, llvm::BasicBlock *block);
  llvm::Value *val(llvm::Value *self, types::Type *type, llvm::BasicBlock *block);
  static UnionType *get(std::vector<types::Type *> types) noexcept;
  // UnionType *clone(Generic *ref) override;
};

} // namespace types
} // namespace seq
