#include "seq/basestage.h"
#include "seq/makerec.h"
#include "seq/call.h"
#include "seq/expr.h"
#include "seq/return.h"
#include "seq/exc.h"
#include "seq/func.h"

using namespace seq;
using namespace llvm;

BaseFunc::BaseFunc() :
    module(nullptr), initBlock(nullptr), preambleBlock(nullptr),
    initFunc(nullptr), func(nullptr), argsVar(true)
{
}

Var *BaseFunc::getArgVar()
{
	return &argsVar;
}

LLVMContext& BaseFunc::getContext()
{
	return module->getContext();
}

BasicBlock *BaseFunc::getInit() const
{
	if (!initBlock)
		throw exc::SeqException("cannot request initialization block before code generation");

	return initBlock;
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

Func::Func(types::Type& inType,
           types::Type& outType,
           std::string name,
           void *rawFunc) :
    BaseFunc(), inType(&inType), outType(&outType),
    pipelines(), outs(new std::map<SeqData, Value *>),
    name(std::move(name)), rawFunc(rawFunc)
{
}

Func::Func(types::Type& inType, types::Type& outType) :
    Func(inType, outType, "", nullptr)
{
}

void BaseFunc::codegenInit(Module *module)
{
	static int idx = 1;

	if (initFunc)
		return;

	LLVMContext& context = module->getContext();

	initFunc = cast<Function>(
	             module->getOrInsertFunction(
	               "init" + std::to_string(idx++),
	               Type::getVoidTy(context)));

	BasicBlock *entryBlock = BasicBlock::Create(context, "entry", initFunc);
	initBlock = BasicBlock::Create(context, "init", initFunc);
	BasicBlock *exitBlock = BasicBlock::Create(context, "exit", initFunc);

	GlobalVariable *init = new GlobalVariable(*module,
	                                          IntegerType::getInt1Ty(context),
	                                          false,
	                                          GlobalValue::PrivateLinkage,
	                                          nullptr,
	                                          "init");

	init->setInitializer(ConstantInt::get(IntegerType::getInt1Ty(context), 0));

	IRBuilder<> builder(entryBlock);
	Value *initVal = builder.CreateLoad(init);
	builder.CreateCondBr(initVal, exitBlock, initBlock);

	builder.SetInsertPoint(initBlock);
	builder.CreateStore(ConstantInt::get(IntegerType::getInt1Ty(context), 1), init);

	builder.SetInsertPoint(exitBlock);
	builder.CreateRetVoid();
}

void BaseFunc::finalizeInit(Module *module)
{
	IRBuilder<> builder(initBlock);
	builder.CreateRetVoid();
}

void Func::codegen(Module *module)
{
	if (!this->module)
		this->module = module;

	if (func)
		return;

	func = inType->makeFuncOf(module, outType);

	if (rawFunc) {
		func->setName(name);
		return;
	}

	if (pipelines.empty())
		throw exc::SeqException("function has no pipelines");

	LLVMContext& context = module->getContext();

	preambleBlock = BasicBlock::Create(context, "preamble", func);
	IRBuilder<> builder(preambleBlock);

	codegenInit(module);
	builder.CreateCall(initFunc);
	inType->setFuncArgs(func, outs, preambleBlock);

	if (!inType->is(types::VoidType::get())) {
		BaseStage& argsBase = BaseStage::make(types::VoidType::get(), inType);
		argsBase.setBase(this);
		argsBase.outs = outs;
		argsVar = argsBase;
	}

	BasicBlock *entry = BasicBlock::Create(context, "entry", func);
	builder.SetInsertPoint(entry);
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

	if (outType->is(types::VoidType::get())) {
		builder.CreateRetVoid();
	} else {
		Stage *tail = pipelines.back().getHead();
		while (!tail->getNext().empty())
			tail = tail->getNext().back();

		if (!dynamic_cast<Return *>(tail)) {  // i.e. if there isn't already a return at the end
			if (!tail->getOutType()->isChildOf(outType))
				throw exc::SeqException("function does not output type '" + outType->getName() + "'");

			ValMap tailOuts = tail->outs;
			Value *result = outType->pack(this, tailOuts, exitBlock);
			builder.CreateRet(result);
		}
	}

	builder.SetInsertPoint(preambleBlock);
	builder.CreateBr(entry);

	finalizeInit(module);
}

Value *Func::codegenCallRaw(BaseFunc *base, ValMap ins, BasicBlock *block)
{
	codegen(block->getModule());
	return inType->callFuncOf(func, ins, block);
}

void Func::codegenCall(BaseFunc *base, ValMap ins, ValMap outs, BasicBlock *block)
{
	Value *result = codegenCallRaw(base, ins, block);
	outType->unpack(base, result, outs, block);
}

void Func::codegenReturn(Expr *expr, BasicBlock*& block)
{
	if (!expr->getType()->isChildOf(outType))
		throw exc::SeqException(
		  "cannot return '" + expr->getType()->getName() + "' from function returning '" + outType->getName() + "'");

	Value *v = expr->codegen(this, block);
	IRBuilder<> builder(block);
	builder.CreateRet(v);

	/*
	 * Can't have anything after the `ret` instruction we just added,
	 * so make a new block and return that to the caller.
	 */
	block = BasicBlock::Create(block->getContext(), "", block->getParent());
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

Var *Func::getArgVar()
{
	if (getInType()->is(types::VoidType::get()))
		throw exc::SeqException("cannot get argument variable of void-input function");

	return &argsVar;
}

types::Type *Func::getInType() const
{
	return inType;
}

types::Type *Func::getOutType() const
{
	return outType;
}

void Func::setInOut(types::Type *inType, types::Type *outType)
{
	this->inType = inType;
	this->outType = outType;
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
	BaseStage& begin = BaseStage::make(types::AnyType::get(), inType, nullptr);
	begin.setBase(this);
	begin.outs = outs;

	Pipeline full = begin | to;
	add(full);

	return full;
}

Pipeline Func::operator|(PipelineList& to)
{
	return *this | MakeRec::make(to);
}

Pipeline Func::operator|(Var& to)
{
	if (rawFunc)
		throw exc::SeqException("cannot add pipelines to native function");

	if (!to.isAssigned())
		throw exc::SeqException("variable used before assigned");

	to.ensureConsistentBase(this);
	Stage *stage = to.getStage();
	BaseStage& begin = BaseStage::make(types::VoidType::get(), to.getType(stage), stage);
	begin.setBase(this);
	begin.outs = to.outs(&begin);
	add(begin);

	return begin;
}

Pipeline Func::operator&(PipelineList& to)
{
	Pipeline first, last;

	for (auto *n = to.head; n; n = n->next) {
		if (n->isVar)
			last = *this | *n->v;
		else
			last = *this | n->p;

		if (n == to.head)
			first = last;
	}

	return {first.getHead(), last.getTail()};
}

Pipeline Func::operator||(Pipeline to)
{
	if (rawFunc)
		throw exc::SeqException("cannot add pipelines to native function");

	if (to.isAdded())
		throw exc::MultiLinkException(*to.getHead());

	to.getHead()->setBase(this);
	BaseStage& begin = BaseStage::make(types::AnyType::get(), inType, nullptr);
	begin.setBase(this);
	begin.outs = outs;

	Pipeline full = begin | to;

	return full;
}

Pipeline Func::operator&&(PipelineList& to)
{
	Pipeline last;

	for (auto *n = to.head; n; n = n->next) {
		if (n->isVar)
			throw exc::SeqException("cannot apply && to pipeline list containing var");
		else {
			Pipeline p = *this || n->p;

			if (n == to.head)
				last = p;
			else
				last = last | p;
		}
	}

	return last;
}

Call& Func::operator()()
{
	return Call::make(*this);
}

BaseFuncLite::BaseFuncLite(Function *func) : BaseFunc()
{
	module = func->getParent();
	initBlock = nullptr;
	preambleBlock = &*func->getBasicBlockList().begin();
	initFunc = nullptr;
	this->func = func;
}

void BaseFuncLite::codegen(Module *module)
{
	throw exc::SeqException("cannot codegen lite base function");
}

void BaseFuncLite::codegenCall(BaseFunc *base,
                               ValMap ins,
                               ValMap outs,
                               BasicBlock *block)
{
	throw exc::SeqException("cannot call lite base function");
}

void BaseFuncLite::codegenReturn(Expr *expr, BasicBlock*& block)
{
	throw exc::SeqException("cannot return from lite base function");
}

void BaseFuncLite::add(Pipeline pipeline)
{
	throw exc::SeqException("cannot add pipelines to lite base function");
}

FuncList::Node::Node(Func& f) :
    f(f), next(nullptr)
{
}

FuncList::FuncList(Func& f)
{
	head = tail = new Node(f);
}

FuncList& FuncList::operator,(Func& f)
{
	auto *n = new Node(f);
	tail->next = n;
	tail = n;
	return *this;
}

FuncList& seq::operator,(Func& f1, Func& f2)
{
	auto& l = *new FuncList(f1);
	l , f2;
	return l;
}

MultiCall& FuncList::operator()()
{
	std::vector<Func *> funcs;
	for (Node *n = head; n; n = n->next)
		funcs.push_back(&n->f);

	return MultiCall::make(funcs);
}
