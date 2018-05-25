#include "seq/base.h"
#include "seq/funct.h"

using namespace seq;
using namespace llvm;

types::FuncType::FuncType(Type *inType, Type *outType) :
    Type(inType->getName() + outType->getName() + "Func", BaseType::get(), SeqData::FUNC),
    inType(inType), outType(outType)
{
}

void types::FuncType::call(BaseFunc *base,
                           ValMap ins,
                           ValMap outs,
                           Value *fn,
                           BasicBlock *block)
{
	IRBuilder<> builder(block);
	Value *result = inType->callFuncOf(fn, ins, block);
	outType->unpack(base, result, outs, block);
}

bool types::FuncType::is(Type *type) const
{
	auto *fnType = dynamic_cast<FuncType *>(type);

	if (!fnType)
		return false;

	return inType->is(fnType->inType) && outType->is(fnType->outType);
}

types::Type *types::FuncType::getCallType()
{
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
