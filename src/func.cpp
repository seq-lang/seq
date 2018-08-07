#include "seq/seq.h"
#include "seq/func.h"

using namespace seq;
using namespace llvm;

BaseFunc::BaseFunc() :
    module(nullptr), preambleBlock(nullptr), func(nullptr)
{
}

LLVMContext& BaseFunc::getContext()
{
	assert(module);
	return module->getContext();
}

BasicBlock *BaseFunc::getPreamble() const
{
	if (!preambleBlock)
		throw exc::SeqException("cannot request preamble before code generation");

	return preambleBlock;
}

types::FuncType *BaseFunc::getFuncType() const
{
	return types::FuncType::get({}, types::VoidType::get());
}

Function *BaseFunc::getFunc()
{
	if (!func)
		throw exc::SeqException("function not yet generated");

	return func;
}

BaseFunc *BaseFunc::clone(types::RefType *ref)
{
	return this;
}

Func::Func() :
    BaseFunc(), name(), inTypes(), outType(), scope(new Block()),
    argNames(), argVars(), gen(false), promise(nullptr), handle(nullptr),
    cleanup(nullptr), suspend(nullptr)
{
	if (!this->argNames.empty())
		assert(this->argNames.size() == this->inTypes.size());
}

Block *Func::getBlock()
{
	return scope;
}

void Func::setGen()
{
	gen = true;
	outType = types::GenType::get(outType);
}

void Func::codegen(Module *module)
{
	if (!this->module)
		this->module = module;

	if (func)
		return;

	LLVMContext& context = module->getContext();

	std::vector<Type *> types;
	for (auto *type : inTypes)
		types.push_back(type->getLLVMType(context));

	static int idx = 1;
	func = cast<Function>(
	         module->getOrInsertFunction(name.empty() ? ("Func." + std::to_string(idx++)) :
	                                                    (name + "." + std::to_string(idx++)),
	                                     FunctionType::get(outType->getLLVMType(context), types, false)));


	preambleBlock = BasicBlock::Create(context, "preamble", func);
	IRBuilder<> builder(preambleBlock);

	/*
	 * Set up general generator intrinsics, if indeed a generator
	 */
	Value *id = nullptr;
	if (gen) {
		promise = makeAlloca(outType->getBaseType(0)->getLLVMType(context), preambleBlock);
		promise->setName("promise");
		Value *promiseRaw = builder.CreateBitCast(promise, IntegerType::getInt8PtrTy(context));
		Function *idFn = Intrinsic::getDeclaration(module, Intrinsic::coro_id);
		Value *nullPtr = ConstantPointerNull::get(IntegerType::getInt8PtrTy(context));
		id = builder.CreateCall(idFn,
		                        {ConstantInt::get(IntegerType::getInt32Ty(context), 0),
		                         promiseRaw,
		                         nullPtr,
		                         nullPtr});
		id->setName("id");
	}

	assert(argNames.empty() || argNames.size() == inTypes.size());
	auto argsIter = func->arg_begin();
	for (unsigned i = 0; i < argNames.size(); i++) {
		auto iter = argVars.find(argNames[i]);
		assert(iter != argVars.end());
		iter->second->setType(inTypes[i]);
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

		Function *allocFunc = cast<Function>(
		                        module->getOrInsertFunction(
		                          ALLOC_FUNC_NAME,
		                          IntegerType::getInt8PtrTy(context),
		                          IntegerType::getIntNTy(context, sizeof(size_t)*8)));

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
		codegenYield(nullptr, nullptr, exitBlock);  // final yield
	} else {
		if (outType->is(types::VoidType::get())) {
			builder.CreateRetVoid();
		} else {
			// i.e. if there isn't already a return at the end
			if (!dynamic_cast<Return *>(scope->stmts.back())) {
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

		cleanup->moveAfter(&func->getBasicBlockList().back());
		suspend->moveAfter(cleanup);
	} else {
		builder.CreateBr(entry);
	}
}

void Func::codegenReturn(Value *val, types::Type *type, BasicBlock*& block)
{
	if (gen)
		throw exc::SeqException("cannot return from generator");

	if (!type->isChildOf(outType))
		throw exc::SeqException(
		  "cannot return '" + type->getName() + "' from function returning '" +
		  outType->getName() + "'");

	if (val) {
		IRBuilder<> builder(block);
		builder.CreateRet(val);
	} else {
		IRBuilder<> builder(block);
		builder.CreateRetVoid();
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

	if (type && !type->isChildOf(outType->getBaseType(0)))
		throw exc::SeqException(
		  "cannot yield '" + type->getName() + "' from generator yielding '" +
		  outType->getBaseType(0)->getName() + "'");

	LLVMContext& context = block->getContext();
	IRBuilder<> builder(block);

	if (val)
		builder.CreateStore(val, promise);

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
	if (iter == argVars.end())
		throw exc::SeqException("function has no argument '" + name + "'");
	return iter->second;
}

types::FuncType *Func::getFuncType() const
{
	return types::FuncType::get(inTypes, outType);
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
	assert(this->inTypes.size() == this->argNames.size());

	argVars.clear();
	for (auto& s : this->argNames)
		argVars.insert({s, new Var()});
}

Func *Func::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (Func *)ref->getClone(this);

	std::vector<types::Type *> inTypesCloned;

	for (auto *type : inTypes)
		inTypesCloned.push_back(type->clone(ref));

	auto *x = new Func();
	ref->addClone(this, x);
	x->name = ref->getName() + "." + name;
	x->argNames = argNames;
	x->inTypes = inTypesCloned;
	x->outType = outType->clone(ref);
	x->scope = scope->clone(ref);

	std::map<std::string, Var *> argVarsCloned;
	for (auto& e : argVars)
		argVarsCloned.insert({e.first, e.second->clone(ref)});
	x->argVars = argVarsCloned;

	x->gen = gen;
	return x;
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
	if (!this->module)
		this->module = module;

	if (func)
		return;

	func = codegenLambda(module);
	preambleBlock = &*func->getBasicBlockList().begin();
}

void BaseFuncLite::codegenReturn(Value *val, types::Type *type, BasicBlock*& block)
{
	throw exc::SeqException("cannot return from lite base function");
}

void BaseFuncLite::codegenYield(Value *val, types::Type *type, BasicBlock*& block)
{
	throw exc::SeqException("cannot yield from lite base function");
}

types::FuncType *BaseFuncLite::getFuncType() const
{
	return types::FuncType::get(inTypes, outType);
}

BaseFuncLite *BaseFuncLite::clone(types::RefType *ref)
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
