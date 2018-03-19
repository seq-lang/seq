#include "any.h"
#include "void.h"

using namespace seq;
using namespace llvm;

types::VoidType::VoidType() : Type("Void", AnyType::get())
{
}

Type *types::VoidType::getLLVMType(LLVMContext& context)
{
	return llvm::Type::getVoidTy(context);
}

types::VoidType *types::VoidType::get()
{
	static types::VoidType instance;
	return &instance;
}
