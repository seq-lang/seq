#pragma once

#include "types/types.h"
#include <iostream>

namespace seq {
namespace types {
class BaseSeqType : public Type {
protected:
  explicit BaseSeqType(std::string name);

public:
  BaseSeqType(BaseSeqType const &) = delete;
  void operator=(BaseSeqType const &) = delete;

  llvm::Value *defaultValue(llvm::BasicBlock *block) override;

  void initFields() override;
  bool isAtomic() const override;
  llvm::Type *getLLVMType(llvm::LLVMContext &context) const override;
  size_t size(llvm::Module *module) const override;

  virtual llvm::Value *make(llvm::Value *ptr, llvm::Value *len,
                            llvm::BasicBlock *block) = 0;
};

class SeqType : public BaseSeqType {
private:
  SeqType();

public:
  llvm::Value *memb(llvm::Value *self, const std::string &name,
                    llvm::BasicBlock *block) override;

  llvm::Value *setMemb(llvm::Value *self, const std::string &name, llvm::Value *val,
                       llvm::BasicBlock *block) override;

  void initOps() override;
  llvm::Value *make(llvm::Value *ptr, llvm::Value *len,
                    llvm::BasicBlock *block) override;
  static SeqType *get() noexcept;
};

class StrType : public BaseSeqType {
private:
  StrType();

public:
  llvm::Value *memb(llvm::Value *self, const std::string &name,
                    llvm::BasicBlock *block) override;

  llvm::Value *setMemb(llvm::Value *self, const std::string &name, llvm::Value *val,
                       llvm::BasicBlock *block) override;

  void initOps() override;
  llvm::Value *make(llvm::Value *ptr, llvm::Value *len,
                    llvm::BasicBlock *block) override;
  static StrType *get() noexcept;
};

class KMer : public Type {
protected:
  unsigned k;
  explicit KMer(unsigned k);

public:
  static const unsigned MAX_LEN = 1024;

  KMer(KMer const &) = delete;
  void operator=(KMer const &) = delete;
  unsigned getK();

  std::string getName() const override;
  llvm::Value *defaultValue(llvm::BasicBlock *block) override;

  void initOps() override;
  bool isAtomic() const override;
  bool is(Type *type) const override;
  llvm::Type *getLLVMType(llvm::LLVMContext &context) const override;
  size_t size(llvm::Module *module) const override;

  KMer *asKMer() override;
  static KMer *get(unsigned k);
};

} // namespace types
} // namespace seq
