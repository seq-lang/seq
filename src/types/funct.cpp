#include "seq/seq.h"
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
	return module->getDataLayout().getTypeAllocSize(getLLVMType(module->getContext()));
}

types::FuncType *types::FuncType::get(std::vector<Type *> inTypes, Type *outType)
{
	return new FuncType(std::move(inTypes), outType);
}

types::FuncType *types::FuncType::clone(Generic *ref)
{
	std::vector<Type *> inTypesCloned;
	for (auto *type : inTypes)
		inTypesCloned.push_back(type->clone(ref));
	return get(inTypesCloned, outType->clone(ref));
}

types::GenType::GenType(Type *outType) :
    Type(outType->getName() + "Gen", BaseType::get(), SeqData::FUNC), outType(outType)
{
	OptionalType *optType = types::OptionalType::get(this->outType);
	addMethod("next", new BaseFuncLite({this}, optType, [this, optType](Module *module) {
		LLVMContext& context = module->getContext();
		auto *f = cast<Function>(module->getOrInsertFunction("seq.gen.next", optType->getLLVMType(context), getLLVMType(context)));
		Value *arg = f->arg_begin();

		BasicBlock *entry = BasicBlock::Create(context, "entry", f);
		BasicBlock *a = BasicBlock::Create(context, "done", f);
		BasicBlock *b = BasicBlock::Create(context, "return", f);

		IRBuilder<> builder(entry);
		resume(arg, entry);
		Value *d = done(arg, entry);
		builder.CreateCondBr(d, a, b);

		builder.SetInsertPoint(a);
		destroy(arg, a);
		builder.CreateRet(optType->make(nullptr, a));

		builder.SetInsertPoint(b);
		Value *val = promise(arg, b);
		builder.CreateRet(optType->make(val, b));

		return f;
	}));
}

Value *types::GenType::defaultValue(BasicBlock *block)
{
	return ConstantPointerNull::get(cast<PointerType>(getLLVMType(block->getContext())));
}

Value *types::GenType::done(Value *self, BasicBlock *block)
{
	Function *doneFn = Intrinsic::getDeclaration(block->getModule(), Intrinsic::coro_done);
	IRBuilder<> builder(block);
	return builder.CreateCall(doneFn, self);
}

void types::GenType::resume(Value *self, BasicBlock *block)
{
	Function *resFn = Intrinsic::getDeclaration(block->getModule(), Intrinsic::coro_resume);
	IRBuilder<> builder(block);
	builder.CreateCall(resFn, self);
}

Value *types::GenType::promise(Value *self, BasicBlock *block)
{
	if (outType->is(types::VoidType::get()))
		return nullptr;

	LLVMContext& context = block->getContext();
	IRBuilder<> builder(block);

	Function *promFn = Intrinsic::getDeclaration(block->getModule(), Intrinsic::coro_promise);

	Value *aln = ConstantInt::get(IntegerType::getInt32Ty(context),
	                              block->getModule()->getDataLayout().getPrefTypeAlignment(outType->getLLVMType(context)));
	Value *from = ConstantInt::get(IntegerType::getInt1Ty(context), 0);

	Value *ptr = builder.CreateCall(promFn, {self, aln, from});
	ptr = builder.CreateBitCast(ptr, PointerType::get(outType->getLLVMType(context), 0));
	return builder.CreateLoad(ptr);
}

void types::GenType::destroy(Value *self, BasicBlock *block)
{
	Function *destFn = Intrinsic::getDeclaration(block->getModule(), Intrinsic::coro_destroy);
	IRBuilder<> builder(block);
	builder.CreateCall(destFn, self);
}

types::Type *types::GenType::getBaseType(seq_int_t idx) const
{
	return outType;
}

bool types::GenType::is(Type *type) const
{
	auto *genType = dynamic_cast<GenType *>(type);
	return genType && outType->is(genType->outType);
}

bool types::GenType::isGeneric(Type *type) const
{
	return (dynamic_cast<GenType *>(type) != nullptr);
}

Type *types::GenType::getLLVMType(LLVMContext &context) const
{
	return IntegerType::getInt8PtrTy(context);
}

seq_int_t types::GenType::size(Module *module) const
{
	return module->getDataLayout().getTypeAllocSize(getLLVMType(module->getContext()));
}

types::GenType *types::GenType::get(Type *outType)
{
	return new GenType(outType);
}

types::GenType *types::GenType::get()
{
	return get(types::BaseType::get());
}

types::GenType *types::GenType::clone(Generic *ref)
{
	return get(outType->clone(ref));
}
