#include "seq/any.h"
#include "seq/exc.h"
#include "seq/void.h"

using namespace seq;
using namespace llvm;

types::VoidType::VoidType() : Type("Void", AnyType::get())
{
}

Value *types::VoidType::loadFromAlloca(BaseFunc *base,
                                       Value *var,
                                       BasicBlock *block)
{
	return nullptr;
}

Value *types::VoidType::storeInAlloca(BaseFunc *base,
                                      Value *self,
                                      BasicBlock *block,
                                      bool storeDefault)
{
	return nullptr;
}

Type *types::VoidType::getLLVMType(LLVMContext& context) const
{
	return llvm::Type::getVoidTy(context);
}

types::VoidType *types::VoidType::get()
{
	static types::VoidType instance;
	return &instance;
}
