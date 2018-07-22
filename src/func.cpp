#include "seq/basestage.h"
#include "seq/makerec.h"
#include "seq/call.h"
#include "seq/expr.h"
#include "seq/exprstage.h"
#include "seq/exc.h"
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

types::Type *BaseFunc::getInType() const
{
	return types::VoidType::get();
}

types::Type *BaseFunc::getOutType() const
{
	return types::VoidType::get();
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

Func::Func(std::string name,
           std::vector<std::string> argNames,
           std::vector<types::Type *> inTypes,
           types::Type *outType) :
    BaseFunc(), inTypes(std::move(inTypes)), outType(outType), pipelines(),
    result(nullptr), argNames(std::move(argNames)), argVars(), gen(false),
    promise(nullptr), handle(nullptr), cleanup(nullptr), suspend(nullptr),
    name(std::move(name)), rawFunc(nullptr)
{
	if (!this->argNames.empty())
		assert(this->argNames.size() == this->inTypes.size());
}

Func::Func(types::Type& inType,
           types::Type& outType,
           std::string name,
           void *rawFunc) :
    Func(std::move(name), {}, {}, &outType)
{
	if (inType.is(types::VoidType::get()))
		inTypes = {};
	else
		inTypes = {&inType};

	this->rawFunc = rawFunc;
}

Func::Func(types::Type& inType, types::Type& outType) :
    Func(inType, outType, "", nullptr)
{
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
	                                                    (rawFunc ? name : name + "." + std::to_string(idx++)),
	                                     FunctionType::get(outType->getLLVMType(context), types, false)));

	if (rawFunc) {
		return;
	}

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
		id = builder.CreateCall(idFn, {ConstantInt::get(IntegerType::getInt32Ty(context), 0), promiseRaw, nullPtr, nullPtr});
		id->setName("id");
	}


	// this is purely for backwards-compatibility with the non-standalone version
	if (!inTypes.empty()) {
		Value *arg = func->arg_begin();
		result = makeAlloca(arg, preambleBlock);
	}

	assert(argNames.empty() || argNames.size() == inTypes.size());
	auto argsIter = func->arg_begin();
	for (unsigned i = 0; i < argNames.size(); i++) {
		BaseStage& argsBase = BaseStage::make(types::VoidType::get(), inTypes[i]);
		argsBase.result = makeAlloca(argsIter, preambleBlock);
		++argsIter;
		argsBase.setBase(this);
		auto iter = argVars.find(argNames[i]);
		assert(iter != argVars.end());
		auto *var = iter->second;
		*var = argsBase;
	}

	BasicBlock *allocBlock = nullptr;
	Value *alloc = nullptr;
	if (gen) {
		allocBlock = BasicBlock::Create(context, "alloc", func);
		builder.SetInsertPoint(allocBlock);
		Function *sizeFn = Intrinsic::getDeclaration(module, Intrinsic::coro_size, {seqIntLLVM(context)});
		Value *size = builder.CreateCall(sizeFn);
		alloc = types::BoolType::get()->alloc(size, allocBlock);  // want i8* so just use Bool for ease
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

	BasicBlock *block;

	for (auto &pipeline : pipelines) {
		pipeline.validate();
		builder.SetInsertPoint(&func->getBasicBlockList().back());
		block = BasicBlock::Create(context, "pipeline", func);
		builder.CreateBr(block);

		auto *begin = dynamic_cast<BaseStage *>(pipeline.getHead());
		assert(begin);
		begin->setBase(pipeline.getHead()->getBase());
		begin->block = block;
		pipeline.getHead()->codegen(module);
	}

	BasicBlock *exitBlock = &func->getBasicBlockList().back();
	builder.SetInsertPoint(exitBlock);

	if (gen) {
		codegenYield(nullptr, nullptr, exitBlock);  // final yield
	} else {
		if (outType->is(types::VoidType::get())) {
			builder.CreateRetVoid();
		} else {
			Stage *tail = pipelines.back().getHead();
			while (!tail->getNext().empty())
				tail = tail->getNext().back();

			if (!dynamic_cast<Return *>(tail)) {  // i.e. if there isn't already a return at the end
				if (tail->getOutType()->isChildOf(outType))
					builder.CreateRet(builder.CreateLoad(tail->result));
				else
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

Value *Func::codegenCall(BaseFunc *base, std::vector<Value *> args, BasicBlock *block)
{
	codegen(block->getModule());
	IRBuilder<> builder(block);
	return builder.CreateCall(func, args);
}

void Func::codegenReturn(Value *val, types::Type *type, BasicBlock*& block)
{
	if (gen)
		throw exc::SeqException("cannot return from generator");

	if (!type->isChildOf(outType))
		throw exc::SeqException(
		  "cannot return '" + type->getName() + "' from function returning '" + outType->getName() + "'");

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
		  "cannot yield '" + type->getName() + "' from generator yielding '" + outType->getBaseType(0)->getName() + "'");

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

void Func::add(Pipeline pipeline)
{
	if (pipeline.isAdded())
		throw exc::MultiLinkException(*pipeline.getHead());

	pipelines.push_back(pipeline);
	pipeline.setAdded();
}

void Func::finalize(Module *module, ExecutionEngine *eng)
{
	if (rawFunc) {
		eng->addGlobalMapping(func, rawFunc);
	} else {
		for (auto &pipeline : pipelines) {
			pipeline.getHead()->finalize(module, eng);
		}
	}
}

bool Func::singleInput() const
{
	return inTypes.size() <= 1;
}

Var *Func::getArgVar(std::string name)
{
	auto iter = argVars.find(name);
	if (iter == argVars.end())
		throw exc::SeqException("function has no argument '" + name + "'");
	return iter->second;
}

types::Type *Func::getInType() const
{
	if (inTypes.size() > 1)
		throw exc::SeqException("function has multiple input types");
	return inTypes.empty() ? types::VoidType::get() : inTypes[0];
}

std::vector<types::Type *> Func::getInTypes() const
{
	return inTypes;
}

types::Type *Func::getOutType() const
{
	return outType;
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
		argVars.insert({s, new Var(true)});
}

void Func::setNative(std::string name, void *rawFunc)
{
	this->name = std::move(name);
	this->rawFunc = rawFunc;
}

Pipeline Func::operator|(Pipeline to)
{
	if (rawFunc)
		throw exc::SeqException("cannot add pipelines to native function");

	if (to.isAdded())
		throw exc::MultiLinkException(*to.getHead());

	to.getHead()->setBase(this);
	BaseStage& begin = BaseStage::make(types::AnyType::get(),
	                                   inTypes.size() > 1 ? types::VoidType::get() : getInType());
	begin.setBase(this);
	begin.deferResult(&result);

	Pipeline full = begin | to;
	add(full);

	return full;
}

Pipeline Func::operator|(PipelineList& to)
{
	return *this | MakeRec::make(to);
}

Call& Func::operator()()
{
	return Call::make(*this);
}

Func *Func::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (Func *)ref->getClone(this);

	std::vector<types::Type *> inTypesCloned;
	std::vector<Pipeline> pipelinesCloned;

	for (auto *type : inTypes)
		inTypesCloned.push_back(type->clone(ref));

	auto *x = new Func(ref->getName() + "." + name, argNames, inTypesCloned, outType->clone(ref));
	ref->addClone(this, x);

	for (auto& pipeline : pipelines)
		pipelinesCloned.push_back(pipeline.clone(ref));

	x->pipelines = pipelinesCloned;

	std::map<std::string, Var *> argVarsCloned;
	for (auto& e : argVars)
		argVarsCloned.insert({e.first, e.second->clone(ref)});
	x->argVars = argVarsCloned;

	x->gen = gen;

	return x;
}

BaseFuncLite::BaseFuncLite(Function *func) : BaseFunc()
{
	module = func->getParent();
	preambleBlock = &*func->getBasicBlockList().begin();
	this->func = func;
}

void BaseFuncLite::codegen(Module *module)
{
	throw exc::SeqException("cannot codegen lite base function");
}

Value *BaseFuncLite::codegenCall(BaseFunc *base, std::vector<Value *> args, BasicBlock *block)
{
	throw exc::SeqException("cannot call lite base function");
}

void BaseFuncLite::codegenReturn(Value *val, types::Type *type, BasicBlock*& block)
{
	throw exc::SeqException("cannot return from lite base function");
}

void BaseFuncLite::codegenYield(Value *val, types::Type *type, BasicBlock*& block)
{
	throw exc::SeqException("cannot yield from lite base function");
}

void BaseFuncLite::add(Pipeline pipeline)
{
	throw exc::SeqException("cannot add pipelines to lite base function");
}
