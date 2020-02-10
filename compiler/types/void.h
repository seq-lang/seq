#pragma once

#include "types/types.h"

namespace seq {
namespace types {
class VoidType : public Type {
private:
  VoidType();

public:
  VoidType(VoidType const &) = delete;
  void operator=(VoidType const &) = delete;

  llvm::Type *getLLVMType(llvm::LLVMContext &context) const override;
  static VoidType *get() noexcept;
};

} // namespace types
} // namespace seq
