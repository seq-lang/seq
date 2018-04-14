#include "seq/any.h"
#include "seq/exc.h"
#include "seq/void.h"

using namespace seq;
using namespace llvm;

types::VoidType::VoidType() : Type("Void", AnyType::get())
{
}

Function *types::VoidType::makeFuncOf(Module *module, Type *outType)
{
	static int idx = 1;
	LLVMContext& context = module->getContext();

	return cast<Function>(
	         module->getOrInsertFunction(
	          getName() + "Func" + std::to_string(idx++),
	          outType->getLLVMType(context)));
}

void types::VoidType::setFuncArgs(Function *func,
                                  ValMap outs,
                                  BasicBlock *block)
{
}

Value *types::VoidType::callFuncOf(Function *func,
                                   ValMap outs,
                                   BasicBlock *block)
{
	IRBuilder<> builder(block);
	return builder.CreateCall(func);
}

Value *types::VoidType::pack(BaseFunc *base,
                             ValMap outs,
                             BasicBlock *block)
{
	return nullptr;
}

void types::VoidType::unpack(BaseFunc *base,
                             Value *value,
                             ValMap outs,
                             BasicBlock *block)
{
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
