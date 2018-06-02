#include "seq/base.h"
#include "seq/funct.h"

using namespace seq;
using namespace llvm;

types::FuncType::FuncType(Type *inType, Type *outType) :
    Type(inType->getName() + outType->getName() + "Func", BaseType::get(), SeqData::FUNC),
    inType(inType), outType(outType)
{
}

Value *types::FuncType::call(BaseFunc *base,
                             Value *self,
                             Value *arg,
                             BasicBlock *block)
{
	return inType->callFuncOf(self, arg, block);
}

Value *types::FuncType::defaultValue(BasicBlock *block)
{
	return ConstantPointerNull::get(cast<PointerType>(getLLVMType(block->getContext())));
}

bool types::FuncType::is(Type *type) const
{
	auto *fnType = dynamic_cast<FuncType *>(type);

	if (!fnType)
		return false;

	return inType->is(fnType->inType) && outType->is(fnType->outType);
}

types::Type *types::FuncType::getCallType(Type *inType)
{
	if (!inType->isChildOf(this->inType))
		throw exc::SeqException(
		  "expected function input type '" + this->inType->getName() + "', but got '" + inType->getName() + "'");

	return outType;
}

Type *types::FuncType::getLLVMType(LLVMContext &context) const
{
	return PointerType::get(inType->getFuncType(context, outType), 0);
}

seq_int_t types::FuncType::size(Module *module) const
{
	std::unique_ptr<DataLayout> layout(new DataLayout(module));
	return layout->getTypeAllocSize(getLLVMType(module->getContext()));
}

types::FuncType *types::FuncType::get(Type *inType, Type *outType)
{
	return new FuncType(inType, outType);
}
