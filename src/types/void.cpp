#include "any.h"
#include "exc.h"
#include "void.h"

using namespace seq;
using namespace llvm;

types::VoidType::VoidType() : Type("Void", AnyType::get())
{
}

Function *types::VoidType::makeFuncOf(Module *module,
                                      ValMap outs,
                                      Type *outType)
{
	static int idx = 1;
	LLVMContext& context = module->getContext();

	Function *func = cast<Function>(
	                   module->getOrInsertFunction(
	                     getName() + "Func" + std::to_string(idx++),
	                     outType->getLLVMType(context)));

	return func;
}

Value *types::VoidType::callFuncOf(llvm::Function *func,
		                           ValMap outs,
                                   llvm::BasicBlock *block)
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

Type *types::VoidType::getLLVMType(LLVMContext& context)
{
	return llvm::Type::getVoidTy(context);
}

types::VoidType *types::VoidType::get()
{
	static types::VoidType instance;
	return &instance;
}
