#include "seq/seq.h"

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
    Type(getFuncName(inTypes), BaseType::get(), Key::FUNC),
    inTypes(std::move(inTypes)), outType(outType)
{
}

unsigned types::FuncType::argCount() const
{
	return (unsigned)inTypes.size();
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
    Type(outType->getName() + "Gen", BaseType::get(), Key::FUNC), outType(outType)
{
	types::Type *type = this->outType->is(&types::Void) ? (types::Type *)&types::Void :
	                                                      types::OptionalType::get(outType);

	addMethod("next", new BaseFuncLite({this}, type, [this, type](Module *module) {
		auto *optType = dynamic_cast<types::OptionalType *>(type);
		LLVMContext& context = module->getContext();
		auto *f = cast<Function>(module->getOrInsertFunction("seq.gen.next",
		                                                     type->getLLVMType(context),
		                                                     getLLVMType(context)));
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
		if (optType)
			builder.CreateRet(optType->make(nullptr, a));
		else
			builder.CreateRetVoid();

		builder.SetInsertPoint(b);
		if (optType) {
			Value *val = promise(arg, b);
			builder.CreateRet(optType->make(val, b));
		} else {
			builder.CreateRetVoid();
		}

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

types::PartialFuncType::PartialFuncType(types::Type *callee, std::vector<types::Type *> callTypes) :
    Type("PartialFunc", BaseType::get(), Key::FUNC), callee(callee), callTypes(std::move(callTypes))
{
	std::vector<types::Type *> types;
	types.push_back(this->callee);
	for (auto *type : this->callTypes) {
		if (type)
			types.push_back(type);
	}
	contents = types::RecordType::get(types);
}

bool types::PartialFuncType::isAtomic() const
{
	return contents->isAtomic();
}

Value *types::PartialFuncType::call(BaseFunc *base,
                                    Value *self,
                                    std::vector<Value *> args,
                                    BasicBlock *block)
{
	IRBuilder<> builder(block);
	std::vector<Value *> argsFull;
	Value *func = contents->memb(self, "1", block);

	unsigned next1 = 2, next2 = 0;
	for (auto *type : callTypes) {
		if (type) {
			argsFull.push_back(contents->memb(self, std::to_string(next1++), block));
		} else {
			assert(next2 < args.size());
			argsFull.push_back(args[next2++]);
		}
	}

	return callee->call(base, func, argsFull, block);
}

Value *types::PartialFuncType::defaultValue(BasicBlock *block)
{
	return contents->defaultValue(block);
}

template <typename T>
static bool nullMatch(std::vector<T *> v1, std::vector<T *> v2)
{
	if (v1.size() != v2.size())
		return false;

	for (unsigned i = 0; i < v1.size(); i++) {
		if ((v1[i] == nullptr) ^ (v2[i] == nullptr))
			return false;
	}

	return true;
}

bool types::PartialFuncType::is(types::Type *type) const
{
	auto *p = dynamic_cast<types::PartialFuncType *>(type);
	return p && nullMatch(callTypes, p->callTypes) && contents->is(p->contents);
}

types::Type *types::PartialFuncType::getCallType(std::vector<types::Type *> inTypes)
{
	std::vector<types::Type *> types(callTypes);
	unsigned next = 0;
	for (auto*& type : types) {
		if (!type) {
			if (next >= inTypes.size())
				throw exc::SeqException("too few arguments passed to partial function call");
			type = inTypes[next++];
		}
	}

	if (next < inTypes.size())
		throw exc::SeqException("too many arguments passed to partial function call");

	return callee->getCallType(types);
}

Type *types::PartialFuncType::getLLVMType(LLVMContext &context) const
{
	return contents->getLLVMType(context);
}

seq_int_t types::PartialFuncType::size(Module *module) const
{
	return contents->size(module);
}

types::PartialFuncType *types::PartialFuncType::get(types::Type *callee, std::vector<types::Type *> callTypes)
{
	return new types::PartialFuncType(callee, std::move(callTypes));
}

Value *types::PartialFuncType::make(Value *func, std::vector<Value *> args, BasicBlock *block)
{
	Value *self = contents->defaultValue(block);
	IRBuilder<> builder(block);
	self = builder.CreateInsertValue(self, func, 0);
	for (unsigned i = 0; i < args.size(); i++)
		self = builder.CreateInsertValue(self, args[i], i + 1);
	return self;
}

types::PartialFuncType *types::PartialFuncType::clone(Generic *ref)
{
	std::vector<types::Type *> callTypesCloned;
	for (auto *type : callTypes)
		callTypesCloned.push_back(type ? type->clone(ref) : nullptr);
	return get(callee->clone(ref), callTypesCloned);
}
