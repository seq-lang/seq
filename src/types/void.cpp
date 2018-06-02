#include "seq/any.h"
#include "seq/exc.h"
#include "seq/void.h"

using namespace seq;
using namespace llvm;

types::VoidType::VoidType() : Type("Void", AnyType::get())
{
}

llvm::Type *types::VoidType::getFuncType(LLVMContext& context, Type *outType)
{
	return FunctionType::get(outType->getLLVMType(context), false);
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

Value *types::VoidType::setFuncArgs(Function *func, BasicBlock *block)
{
	return nullptr;
}

Value *types::VoidType::callFuncOf(llvm::Value *func,
                                   llvm::Value *arg,
                                   llvm::BasicBlock *block)
{
	IRBuilder<> builder(block);
	return builder.CreateCall(func);
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
