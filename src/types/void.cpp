#include "seq/seq.h"

using namespace seq;
using namespace llvm;

types::VoidType::VoidType() : Type("Void", AnyType::get(), true)
{
}

Type *types::VoidType::getLLVMType(LLVMContext& context) const
{
	return llvm::Type::getVoidTy(context);
}

types::VoidType *types::VoidType::get() noexcept
{
	static types::VoidType instance;
	return &instance;
}
