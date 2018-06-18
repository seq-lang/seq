#include "seq/base.h"
#include "seq/funct.h"

using namespace seq;
using namespace llvm;

static std::string getFuncName(std::vector<types::Type *> inTypes)
{
	std::string name;
	for (auto *type : inTypes)
		name += type->getName();
	name += "Func";
	return name;
}

types::FuncType::FuncType(std::vector<types::Type *> inTypes, Type *outType) :
    Type(getFuncName(inTypes), BaseType::get(), SeqData::FUNC),
    inTypes(std::move(inTypes)), outType(outType)
{
}

Value *types::FuncType::call(BaseFunc *base,
                             Value *self,
                             std::vector<Value *> args,
                             BasicBlock *block)
{
	IRBuilder<> builder(block);
	return builder.CreateCall(self, args);
}

Value *types::FuncType::defaultValue(BasicBlock *block)
{
	return ConstantPointerNull::get(cast<PointerType>(getLLVMType(block->getContext())));
}

bool types::FuncType::is(Type *type) const
{
	auto *fnType = dynamic_cast<FuncType *>(type);

	if (!fnType || !outType->is(fnType->outType) || inTypes.size() != fnType->inTypes.size())
		return false;

	for (unsigned i = 0; i < inTypes.size(); i++)
		if (!inTypes[i]->is(fnType->inTypes[i]))
			return false;

	return true;
}

types::Type *types::FuncType::getCallType(std::vector<Type *> inTypes)
{
	if (this->inTypes.size() != inTypes.size())
		throw exc::SeqException("expected " + std::to_string(this->inTypes.size()) + " arguments, but got " + std::to_string(inTypes.size()));

	for (unsigned i = 0; i < inTypes.size(); i++)
		if (!inTypes[i]->isChildOf(this->inTypes[i]))
			throw exc::SeqException(
			  "expected function input type '" + this->inTypes[i]->getName() + "', but got '" + inTypes[i]->getName() + "'");

	return outType;
}

Type *types::FuncType::getLLVMType(LLVMContext &context) const
{
	std::vector<llvm::Type *> types;
	for (auto *type : inTypes)
		types.push_back(type->getLLVMType(context));

	return PointerType::get(FunctionType::get(outType->getLLVMType(context), types, false), 0);
}

seq_int_t types::FuncType::size(Module *module) const
{
	std::unique_ptr<DataLayout> layout(new DataLayout(module));
	return layout->getTypeAllocSize(getLLVMType(module->getContext()));
}

types::FuncType *types::FuncType::get(std::vector<Type *> inTypes, Type *outType)
{
	return new FuncType(std::move(inTypes), outType);
}

types::FuncType *types::FuncType::clone(types::RefType *ref)
{
	std::vector<Type *> inTypesCloned;
	for (auto *type : inTypes)
		inTypesCloned.push_back(type->clone(ref));
	return get(inTypesCloned, outType->clone(ref));
}
