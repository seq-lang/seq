#include "lang/seq.h"

using namespace seq;
using namespace llvm;

types::VoidType::VoidType() : Type("void", AnyType::get(), true) {}

Type *types::VoidType::getLLVMType(LLVMContext &context) const {
  return llvm::Type::getVoidTy(context);
}

types::VoidType *types::VoidType::get() noexcept {
  static types::VoidType instance;
  return &instance;
}
