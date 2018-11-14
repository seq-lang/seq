#include "seq/seq.h"

using namespace seq;
using namespace llvm;

BaseFunc::BaseFunc() :
    parentType(nullptr), module(nullptr), preambleBlock(nullptr), func(nullptr)
{
}

void BaseFunc::resolveTypes()
{
}

LLVMContext& BaseFunc::getContext()
{
	assert(module);
	return module->getContext();
}

BasicBlock *BaseFunc::getPreamble() const
{
	assert(preambleBlock);
	return preambleBlock;
}

types::FuncType *BaseFunc::getFuncType() const
{
	return types::FuncType::get({}, types::Void);
}

Function *BaseFunc::getFunc()
{
	assert(func);
	return func;
}

void BaseFunc::setEnclosingClass(types::Type *parentType)
{
	this->parentType = parentType;
}

BaseFunc *BaseFunc::clone(Generic *ref)
{
	return this;
}

Func::Func() :
    BaseFunc(), Generic(false), SrcObject(), external(false), name(), inTypes(), outType(types::Void),
    scope(new Block()), argNames(), argVars(), parentFunc(nullptr), ret(nullptr), yield(nullptr),
    resolvingTypes(false), gen(false), promise(nullptr), handle(nullptr), cleanup(nullptr), suspend(nullptr)
{
	if (!this->argNames.empty())
		assert(this->argNames.size() == this->inTypes.size());
}

Block *Func::getBlock()
{
	return scope;
}

std::string Func::genericName()
{
	return name;
}

Func *Func::realize(std::vector<types::Type *> types)
{
	Generic *x = realizeGeneric(std::move(types));
	auto *func = dynamic_cast<Func *>(x);
	assert(func);
	func->resolveTypes();
	return func;
}

std::vector<types::Type *> Func::deduceTypesFromArgTypes(std::vector<types::Type *> argTypes)
{
	return Generic::deduceTypesFromArgTypes(inTypes, argTypes);
}

void Func::setEnclosingFunc(Func *parentFunc)
{
	this->parentFunc = parentFunc;
}

void Func::sawReturn(Return *ret)
{
	if (this->ret)
		return;

	this->ret = ret;
}

void Func::sawYield(Yield *yield)
{
	if (this->yield)
		return;

	this->yield = yield;
	gen = true;
	outType = types::GenType::get(outType);
}

/*
 * Mangling rules:
 *   - Base function name is mangled as "<name>[<generic type 1>,<generic type 2>,(...),<generic type N>]" or
 *     simply "<name>" if function is not generic.
 *   - If function is nested in function g, "<mangled name of g>::" is prepended to the name.
 *   - If function is method of class C, "<type name of C>::" is prepended to the name.
 *   - ".<out type>.<arg type 1>.<arg type 2>.(...).<arg type N>" is appended to the name.
 */
std::string Func::getMangledFuncName()
{
	if (external)
		return name;

	// a nested function can't be a class method:
	assert(!(parentType && parentFunc));

	std::string mangled = name;

	if (numGenerics() > 0) {
		mangled += "[";
		for (unsigned i = 0; i < numGenerics(); i++) {
			mangled += getGeneric(i)->getName();
			if (i < numGenerics() - 1)
				mangled += ",";
		}
		mangled += "]";
	}

	if (parentFunc)
		mangled = parentFunc->getMangledFuncName() + "::" + mangled;

	if (parentType)
		mangled = parentType->getName() + "::" + mangled;

	types::FuncType *funcType = getFuncType();
	for (unsigned i = 0; i < funcType->numBaseTypes(); i++)
		mangled += "." + funcType->getBaseType(i)->getName();

	return mangled;
}

void Func::resolveTypes()
{
	if (external || resolvingTypes)
		return;

	resolvingTypes = true;

	try {
		scope->resolveTypes();

		// return type deduction
		if ((outType->is(types::Void) || outType->is(types::GenType::get(types::Void))) && (yield || (ret && ret->getExpr()))) {
			if (yield) {
				outType = types::GenType::get(yield->getExpr() ? yield->getExpr()->getType() : types::Void);
			} else if (ret) {
				outType = ret->getExpr() ? ret->getExpr()->getType() : types::Void;
			} else {
				assert(0);
			}
		}
	} catch (exc::SeqException&) {
		/*
		 * Function had some generic types which could not be resolved yet; not a real issue
		 * though, since these will be resolved whenever the generics are instantiated, so we
		 * catch this exception and ignore it.
		 */
	}

	resolvingTypes = false;
}

void Func::codegen(Module *module)
{
	if (this->module != module) {
		func = nullptr;
		this->module = module;
	}

	if (func)
		return;

	resolveTypes();
	LLVMContext& context = module->getContext();
	std::vector<Type *> types;
	for (auto *type : inTypes)
		types.push_back(type->getLLVMType(context));

	std::string mangledName = getMangledFuncName();
	auto *cached = module->getFunction(mangledName);
	if (cached) {
		func = cast<Function>(cached);
		return;
	}

	FunctionType *funcTypeLLVM = FunctionType::get(outType->getLLVMType(context), types, false);
	func = cast<Function>(module->getOrInsertFunction(mangledName, funcTypeLLVM));

	if (external)
		return;

	func->setLinkage(GlobalValue::PrivateLinkage);
	preambleBlock = BasicBlock::Create(context, "preamble", func);
	IRBuilder<> builder(preambleBlock);

	/*
	 * Set up general generator intrinsics, if indeed a generator
	 */
	Value *id = nullptr;
	if (gen) {
		Function *idFn = Intrinsic::getDeclaration(module, Intrinsic::coro_id);
		Value *nullPtr = ConstantPointerNull::get(IntegerType::getInt8PtrTy(context));

		if (!outType->getBaseType(0)->is(types::Void)) {
			promise = makeAlloca(outType->getBaseType(0)->getLLVMType(context), preambleBlock);
			promise->setName("promise");
			Value *promiseRaw = builder.CreateBitCast(promise, IntegerType::getInt8PtrTy(context));
			id = builder.CreateCall(idFn,
			                        {ConstantInt::get(IntegerType::getInt32Ty(context), 0),
			                         promiseRaw,
			                         nullPtr,
			                         nullPtr});
		} else {
			id = builder.CreateCall(idFn,
			                        {ConstantInt::get(IntegerType::getInt32Ty(context), 0),
			                         nullPtr,
			                         nullPtr,
			                         nullPtr});
		}
		id->setName("id");
	}

	assert(argNames.empty() || argNames.size() == inTypes.size());
	auto argsIter = func->arg_begin();
	for (auto& argName : argNames) {
		auto iter = argVars.find(argName);
		assert(iter != argVars.end());
		iter->second->store(this, argsIter, preambleBlock);
		++argsIter;
	}

	BasicBlock *allocBlock = nullptr;
	Value *alloc = nullptr;
	if (gen) {
		allocBlock = BasicBlock::Create(context, "alloc", func);
		builder.SetInsertPoint(allocBlock);
		Function *sizeFn = Intrinsic::getDeclaration(module, Intrinsic::coro_size, {seqIntLLVM(context)});
		Value *size = builder.CreateCall(sizeFn);
		auto *allocFunc = makeAllocFunc(module, false);
		alloc = builder.CreateCall(allocFunc, size);
	}

	BasicBlock *entry = BasicBlock::Create(context, "entry", func);
	BasicBlock *entryActual = entry;

	if (gen) {
		builder.CreateBr(entry);
		builder.SetInsertPoint(entry);
		PHINode *phi = builder.CreatePHI(IntegerType::getInt8PtrTy(context), 2);
		phi->addIncoming(ConstantPointerNull::get(IntegerType::getInt8PtrTy(context)), preambleBlock);
		phi->addIncoming(alloc, allocBlock);

		Function *beginFn = Intrinsic::getDeclaration(module, Intrinsic::coro_begin);
		handle = builder.CreateCall(beginFn, {id, phi});
		handle->setName("hdl");

		/*
		 * Cleanup code
		 */
		cleanup = BasicBlock::Create(context, "cleanup", func);
		builder.SetInsertPoint(cleanup);
		Function *freeFn = Intrinsic::getDeclaration(module, Intrinsic::coro_free);
		builder.CreateCall(freeFn, {id, handle});

		suspend = BasicBlock::Create(context, "suspend", func);
		builder.CreateBr(suspend);
		builder.SetInsertPoint(suspend);

		Function *endFn = Intrinsic::getDeclaration(module, Intrinsic::coro_end);
		builder.CreateCall(endFn, {handle, ConstantInt::get(IntegerType::getInt1Ty(context), 0)});
		builder.CreateRet(handle);

		exit = BasicBlock::Create(context, "final", func);
	}

	builder.SetInsertPoint(entry);

	if (gen) {
		// make sure the generator is initially suspended:
		codegenYield(nullptr, outType->getBaseType(0), entry);
	}

	BasicBlock *block = entry;
	scope->codegen(block);

	BasicBlock *exitBlock = block;
	builder.SetInsertPoint(exitBlock);

	if (gen) {
		builder.CreateBr(exit);
		codegenYield(nullptr, nullptr, exit);  // final yield
	} else {
		if (outType->is(types::Void)) {
			builder.CreateRetVoid();
		} else {
			// i.e. if there isn't already a return at the end
			if (scope->stmts.empty() || !dynamic_cast<Return *>(scope->stmts.back())) {
				builder.CreateRet(outType->defaultValue(exitBlock));
			} else {
				builder.CreateUnreachable();
			}
		}
	}

	builder.SetInsertPoint(preambleBlock);
	if (gen) {
		Function *allocFn = Intrinsic::getDeclaration(module, Intrinsic::coro_alloc);
		Value *needAlloc = builder.CreateCall(allocFn, id);
		builder.CreateCondBr(needAlloc, allocBlock, entryActual);

		exit->moveAfter(&func->getBasicBlockList().back());
		cleanup->moveAfter(exit);
		suspend->moveAfter(cleanup);
	} else {
		builder.CreateBr(entry);
	}
}

void Func::codegenReturn(Value *val, types::Type *type, BasicBlock*& block)
{
	if (gen && val)
		throw exc::SeqException("cannot return value from generator");

	if (val && type && !types::is(type, outType))
		throw exc::SeqException(
		  "cannot return '" + type->getName() + "' from function returning '" +
		  outType->getName() + "'");

	if (val && type && type->is(types::Void))
		throw exc::SeqException("cannot return void value from function");

	IRBuilder<> builder(block);

	if (gen) {
		builder.CreateBr(exit);
	} else {
		if (val) {
			builder.CreateRet(val);
		} else {
			builder.CreateRetVoid();
		}
	}

	/*
	 * Can't have anything after the `ret` instruction we just added,
	 * so make a new block and return that to the caller.
	 */
	block = BasicBlock::Create(block->getContext(), "", block->getParent());
}

// type = nullptr means final yield
void Func::codegenYield(Value *val, types::Type *type, BasicBlock*& block)
{
	if (!gen)
		throw exc::SeqException("cannot yield from a non-generator");

	if (type && !types::is(type, outType->getBaseType(0)))
		throw exc::SeqException(
		  "cannot yield '" + type->getName() + "' from generator yielding '" +
		  outType->getBaseType(0)->getName() + "'");

	if (val && type && type->is(types::Void))
		throw exc::SeqException("cannot yield void value from generator");

	LLVMContext& context = block->getContext();
	IRBuilder<> builder(block);

	if (val) {
		assert(promise);
		builder.CreateStore(val, promise);
	}

	Function *suspFn = Intrinsic::getDeclaration(module, Intrinsic::coro_suspend);
	Value *tok = ConstantTokenNone::get(context);
	Value *final = ConstantInt::get(IntegerType::getInt1Ty(context), type ? 0 : 1);
	Value *susp = builder.CreateCall(suspFn, {tok, final});

	/*
	 * Can't have anything after the `ret` instruction we just added,
	 * so make a new block and return that to the caller.
	 */
	block = BasicBlock::Create(block->getContext(), "", block->getParent());

	SwitchInst *inst = builder.CreateSwitch(susp, suspend, 2);
	inst->addCase(ConstantInt::get(IntegerType::getInt8Ty(context), 0), block);
	inst->addCase(ConstantInt::get(IntegerType::getInt8Ty(context), 1), cleanup);

	if (!type) {
		builder.SetInsertPoint(block);
		builder.CreateUnreachable();
	}
}

Var *Func::getArgVar(std::string name)
{
	auto iter = argVars.find(name);
	assert(iter != argVars.end());
	return iter->second;
}

types::FuncType *Func::getFuncType() const
{
	return types::FuncType::get(inTypes, outType);
}

void Func::setExternal()
{
	external = true;
}

void Func::setIns(std::vector<types::Type *> inTypes)
{
	this->inTypes = std::move(inTypes);
}

void Func::setOut(types::Type *outType)
{
	this->outType = outType;
}

void Func::setName(std::string name)
{
	this->name = std::move(name);
}

void Func::setArgNames(std::vector<std::string> argNames)
{
	this->argNames = std::move(argNames);
	assert(inTypes.size() == this->argNames.size());

	argVars.clear();
	for (unsigned i = 0; i < this->argNames.size(); i++)
		argVars.insert({this->argNames[i], new Var(inTypes[i])});
}

Func *Func::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (Func *)ref->getClone(this);

	auto *x = new Func();
	ref->addClone(this, x);
	setCloneBase(x, ref);

	std::vector<types::Type *> inTypesCloned;
	for (auto *type : inTypes)
		inTypesCloned.push_back(type->clone(ref));

	x->external = external;
	x->name = name;
	x->argNames = argNames;
	x->inTypes = inTypesCloned;
	x->outType = outType->clone(ref);
	x->scope = scope->clone(ref);

	std::map<std::string, Var *> argVarsCloned;
	for (auto& e : argVars)
		argVarsCloned.insert({e.first, e.second->clone(ref)});
	x->argVars = argVarsCloned;

	if (parentType) x->parentType = parentType->clone(ref);
	if (parentFunc) x->parentFunc = parentFunc->clone(ref);
	if (ret) x->ret = ret->clone(ref);
	if (yield) x->yield = yield->clone(ref);
	x->gen = gen;
	SEQ_RETURN_CLONE(x);
}

BaseFuncLite::BaseFuncLite(std::vector<types::Type *> inTypes,
                           types::Type *outType,
                           std::function<llvm::Function *(llvm::Module *)> codegenLambda) :
    BaseFunc(), inTypes(std::move(inTypes)),
    outType(outType), codegenLambda(std::move(codegenLambda))
{
}

void BaseFuncLite::codegen(Module *module)
{
	if (this->module != module) {
		func = nullptr;
		this->module = module;
	}

	if (func)
		return;

	func = codegenLambda(module);
	preambleBlock = &*func->getBasicBlockList().begin();
}

types::FuncType *BaseFuncLite::getFuncType() const
{
	return types::FuncType::get(inTypes, outType);
}

BaseFuncLite *BaseFuncLite::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (BaseFuncLite *)ref->getClone(this);

	std::vector<types::Type *> inTypesCloned;

	for (auto *type : inTypes)
		inTypesCloned.push_back(type->clone(ref));

	auto *x = new BaseFuncLite(inTypesCloned, outType->clone(ref), codegenLambda);
	ref->addClone(this, x);

	return x;
}
