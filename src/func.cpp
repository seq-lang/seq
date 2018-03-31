#include "basestage.h"
#include "exc.h"
#include "func.h"

using namespace seq;
using namespace llvm;

BaseFunc::BaseFunc() :
    compilationContext(), module(nullptr), initBlock(nullptr),
    preambleBlock(nullptr), initFunc(nullptr), func(nullptr)
{
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

Func::Func(types::Type& inType, types::Type& outType) :
    BaseFunc(), inType(&inType), outType(&outType),
    pipelines(), outs(new std::map<SeqData, Value *>)
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
	if (func)
		return;

	if (pipelines.empty())
		throw exc::SeqException("function has no pipelines");

	compilationContext.reset();
	LLVMContext& context = module->getContext();

	func = inType->makeFuncOf(module, outs, outType);

	preambleBlock = BasicBlock::Create(context, "preamble", func);
	IRBuilder<> builder(preambleBlock);

	codegenInit(module);
	builder.CreateCall(initFunc);

	BasicBlock *entry = BasicBlock::Create(context, "entry", func);
	builder.SetInsertPoint(entry);
	BasicBlock *block;

	compilationContext.inFunc = true;
	compilationContext.inMain = true;
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
	compilationContext.inMain = false;

	Stage *tail = pipelines.back().getHead();
	while (!tail->getNext().empty())
		tail = tail->getNext().back();

	if (!tail->getOutType()->isChildOf(outType))
		throw exc::SeqException("function does not output type '" + outType->getName() + "'");

	ValMap tailOuts = tail->outs;
	BasicBlock *exitBlock = &func->getBasicBlockList().back();
	builder.SetInsertPoint(exitBlock);
	Value *result = outType->pack(this, tailOuts, exitBlock);
	builder.CreateRet(result);

	builder.SetInsertPoint(preambleBlock);
	builder.CreateBr(entry);

	finalizeInit(module);
}

void Func::codegenCall(BaseFunc *base, ValMap ins, ValMap outs, BasicBlock *block)
{
	module = block->getModule();
	codegen(module);
	Value *result = inType->callFuncOf(func, ins, block);
	outType->unpack(base, result, outs, block);
}

void Func::add(Pipeline pipeline)
{
	if (pipeline.isAdded())
		throw exc::MultiLinkException(*pipeline.getHead());

	pipelines.push_back(pipeline);
	pipeline.setAdded();
}

types::Type *Func::getInType() const
{
	return inType;
}

types::Type *Func::getOutType() const
{
	return outType;
}

Pipeline Func::operator|(Pipeline to)
{
	to.getHead()->setBase(this);
	BaseStage& begin = BaseStage::make(types::VoidType::get(), inType, nullptr);
	begin.setBase(this);
	begin.outs = outs;

	Pipeline full = begin | to;
	add(full);

	return full;
}

Pipeline Func::operator|(PipelineList to)
{
	for (auto *node = to.head; node; node = node->next) {
		*this | node->p;
	}

	return {to.head->p.getHead(), to.tail->p.getTail()};
}

Pipeline Func::operator|(Var& to)
{
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
