#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include "seq/common.h"
#include "seq/basestage.h"
#include "seq/util.h"
#include "seq/exc.h"
#include "seq/seq.h"

using namespace seq;
using namespace llvm;

PipelineAggregator::PipelineAggregator(SeqModule *base) : base(base), pipelines()
{
}

void PipelineAggregator::add(Pipeline pipeline)
{
	if (pipeline.isAdded())
		throw exc::MultiLinkException(*pipeline.getHead());

	pipelines.push_back(pipeline);
	pipeline.setAdded();
}

Pipeline PipelineAggregator::addWithIndex(Pipeline to, seq_int_t idx, bool addFull)
{
	idx -= 1;  // 1-based to 0-based
	if (idx < 0 || idx >= (seq_int_t)io::MAX_INPUTS)
		throw exc::SeqException("invalid sequence index specified");

	to.getHead()->setBase(base);
	types::Type *outType = base->standalone ? (types::Type *)types::VoidType::get() :
	                                          (types::Type *)types::SeqType::get();
	BaseStage& begin = addFull ? BaseStage::make(types::VoidType::get(), outType) :
	                             BaseStage::make(types::AnyType::get(), outType);
	begin.setBase(base);

	if (!base->standalone)
		begin.outs = base->outs[idx];

	Pipeline full = begin | to;
	if (addFull)
		add(full);

	return full;
}

Pipeline PipelineAggregator::operator|(Pipeline to)
{
	return addWithIndex(to, 1);
}

Pipeline PipelineAggregator::operator|(PipelineList& to)
{
	return *this | MakeRec::make(to);
}

Pipeline PipelineAggregator::operator|(Var& to)
{
	if (!to.isAssigned())
		throw exc::SeqException("variable used before assigned");

	to.ensureConsistentBase(base);
	Stage *stage = to.getStage();
	BaseStage& begin = BaseStage::make(types::VoidType::get(), to.getType(stage), stage);
	begin.setBase(base);
	begin.outs = to.outs(&begin);
	add(begin);

	return begin;
}

Pipeline PipelineAggregator::operator&(PipelineList& to)
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

Pipeline PipelineAggregator::operator||(Pipeline to)
{
	return addWithIndex(to, 1, false);
}

Pipeline PipelineAggregator::operator&&(PipelineList& to)
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

Pipeline PipelineAggregatorProxy::operator|(Pipeline to)
{
	return aggr.addWithIndex(to, idx);
}

Pipeline PipelineAggregatorProxy::operator|(PipelineList& to)
{
	return *this | MakeRec::make(to);
}

Pipeline PipelineAggregatorProxy::operator|(Var& to)
{
	return aggr | to;
}

Pipeline PipelineAggregatorProxy::operator&(PipelineList& to)
{
	Pipeline first, last;

	for (auto *n = to.head; n; n = n->next) {
		if (n->isVar)
			last = aggr | *n->v;
		else
			last = aggr.addWithIndex(n->p, idx);

		if (n == to.head)
			first = last;
	}

	return {first.getHead(), last.getTail()};
}

Pipeline PipelineAggregatorProxy::operator||(Pipeline to)
{
	return aggr.addWithIndex(to, idx, false);
}

Pipeline PipelineAggregatorProxy::operator&&(PipelineList& to)
{
	Pipeline last;

	for (auto *n = to.head; n; n = n->next) {
		if (n->isVar)
			throw exc::SeqException("cannot apply && to pipeline list containing var");
		else {
			Pipeline p = aggr.addWithIndex(n->p, idx, false);

			if (n == to.head)
				last = p;
			else
				last = last | p;
		}
	}

	return last;
}

PipelineAggregatorProxy::PipelineAggregatorProxy(PipelineAggregator& aggr, seq_int_t idx) :
    aggr(aggr), idx(idx)
{
}

PipelineAggregatorProxy::PipelineAggregatorProxy(PipelineAggregator& aggr) :
    PipelineAggregatorProxy(aggr, 1)
{
}


SeqModule::SeqModule(bool standalone) :
    BaseFunc(), standalone(standalone), sources(), argsVar(true),
    main(this), once(this), last(this), data(nullptr)
{
	if (!standalone)
		for (auto& out : outs)
			out = makeValMap();
}

SeqModule::~SeqModule()
{
	delete data;
}

void SeqModule::source(std::string source)
{
	if (standalone)
		throw exc::SeqException("cannot add source to standalone module");

	sources.push_back(source);
}

Var *SeqModule::getArgsVar()
{
	if (!standalone)
		throw exc::SeqException("cannot get argument variable in non-standalone mode");

	return &argsVar;
}

void SeqModule::codegen(Module *module)
{
	if (func)
		return;

	compilationContext.reset();
	LLVMContext& context = module->getContext();
	this->module = module;

	types::Type *argsType = nullptr;
	Value *args = nullptr;
	Value *seqs = nullptr;
	Value *isLast = nullptr;

	if (standalone) {
		argsType = types::ArrayType::get(types::StrType::get());

		func = cast<Function>(
		         module->getOrInsertFunction(
		           "main",
		           Type::getVoidTy(context),
		           argsType->getLLVMType(context)));

		auto argiter = func->arg_begin();
		args = argiter;
	} else {
		func = cast<Function>(
				module->getOrInsertFunction(
						"main",
						Type::getVoidTy(context),
						PointerType::get(types::Seq.getLLVMType(context), 0),
						IntegerType::getInt8Ty(context)));

		auto argiter = func->arg_begin();
		seqs = argiter++;
		isLast = argiter;
		isLast->setName("last");
	}

	/* preamble */
	preambleBlock = BasicBlock::Create(context, "preamble", func);
	IRBuilder<> builder(preambleBlock);

	codegenInit(module);
	builder.CreateCall(initFunc);

	if (standalone) {
		assert(argsType != nullptr);
		BaseStage& argsBase = BaseStage::make(types::VoidType::get(), argsType);
		argsType->unpack(this, args, argsBase.outs, preambleBlock);
		argsVar = argsBase;
	} else {
		for (size_t i = 0; i < sources.size(); i++) {
			types::Seq.codegenLoad(this,
			                       outs[i],
			                       preambleBlock,
			                       seqs,
			                       ConstantInt::get(seqIntLLVM(context), i));
		}
	}

	BasicBlock *onceBr = nullptr;
	BasicBlock *origOnceBlock = nullptr;
	BasicBlock *onceBlock = nullptr;
	GlobalVariable *init = nullptr;

	if (!standalone) {
		/* one-time execution */
		onceBr = BasicBlock::Create(context, "oncebr", func);
		origOnceBlock = BasicBlock::Create(context, "once", func);
		onceBlock = origOnceBlock;  // onceBlock is really the _last_ once-block
		builder.SetInsertPoint(onceBlock);

		compilationContext.inOnce = true;
		for (auto &pipeline : once.pipelines) {
			pipeline.validate();
			builder.SetInsertPoint(&func->getBasicBlockList().back());
			onceBlock = BasicBlock::Create(context, "pipeline", func);
			builder.CreateBr(onceBlock);

			auto *begin = dynamic_cast<BaseStage *>(pipeline.getHead());
			assert(begin);
			begin->setBase(pipeline.getHead()->getBase());
			begin->block = onceBlock;
			pipeline.getHead()->codegen(module);
		}
		compilationContext.inOnce = false;

		onceBlock = &func->getBasicBlockList().back();

		init = new GlobalVariable(*module,
		                          IntegerType::getInt1Ty(context),
		                          false,
		                          GlobalValue::PrivateLinkage,
		                          nullptr,
		                          "init");

		init->setInitializer(ConstantInt::get(IntegerType::getInt1Ty(context), 0));
	}

	/* main */
	BasicBlock *entry = BasicBlock::Create(context, "entry", func);
	BasicBlock *block;

	compilationContext.inMain = true;
	for (auto &pipeline : main.pipelines) {
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

	BasicBlock *lastMain = nullptr;
	BasicBlock *lastBr = nullptr;
	BasicBlock *origLastBlock = nullptr;
	BasicBlock *lastBlock = nullptr;

	if (!standalone) {
		/* last */
		lastMain = &func->getBasicBlockList().back();
		lastBr = BasicBlock::Create(context, "lastbr", func);
		origLastBlock = BasicBlock::Create(context, "last", func);
		lastBlock = origLastBlock;  // lastBlock is really the _last_ last-block

		compilationContext.inLast = true;
		for (auto &pipeline : last.pipelines) {
			pipeline.validate();
			builder.SetInsertPoint(&func->getBasicBlockList().back());
			lastBlock = BasicBlock::Create(context, "pipeline", func);
			builder.CreateBr(lastBlock);

			auto *begin = dynamic_cast<BaseStage *>(pipeline.getHead());
			assert(begin);
			begin->setBase(pipeline.getHead()->getBase());
			begin->block = lastBlock;
			pipeline.getHead()->codegen(module);
		}
		compilationContext.inLast = false;
	}

	lastBlock = &func->getBasicBlockList().back();
	BasicBlock *exit = BasicBlock::Create(context, "exit", func);

	/* stitch it all together */
	builder.SetInsertPoint(preambleBlock);
	builder.CreateBr(standalone ? entry : onceBr);

	if (!standalone) {
		builder.SetInsertPoint(onceBr);
		builder.CreateCondBr(builder.CreateLoad(init), entry, origOnceBlock);

		builder.SetInsertPoint(onceBlock);
		builder.CreateStore(ConstantInt::get(IntegerType::getInt1Ty(context), 1), init);
		builder.CreateBr(entry);

		builder.SetInsertPoint(lastMain);
		builder.CreateBr(lastBr);

		builder.SetInsertPoint(lastBr);
		builder.CreateCondBr(isLast, origLastBlock, exit);
	}

	builder.SetInsertPoint(lastBlock);
	builder.CreateBr(exit);

	builder.SetInsertPoint(exit);
	builder.CreateRetVoid();

	finalizeInit(module);
}

void SeqModule::codegenCall(BaseFunc *base, ValMap ins, ValMap outs, BasicBlock *block)
{
	throw exc::SeqException("cannot call Seq instance");
}

void SeqModule::execute(const std::vector<std::string>& args, bool debug)
{
	try {
		if (!args.empty() && !standalone)
			throw exc::SeqException("cannot only pass arguments in standalone mode");

		io::Format fmt = io::Format::TXT;

		if (!standalone) {
			if (sources.empty())
				throw exc::SeqException("sequence source not specified");

			if (sources.size() > io::MAX_INPUTS)
				throw exc::SeqException("too many inputs (max: " + std::to_string(io::MAX_INPUTS) + ")");

			fmt = io::extractExt(sources[0]);

			for (const auto &src : sources) {
				if (io::extractExt(src) != fmt)
					throw exc::SeqException("inconsistent input formats");
			}
		}

		LLVMContext context;
		InitializeNativeTarget();
		InitializeNativeTargetAsmPrinter();

		std::unique_ptr<Module> owner(new Module("seq", context));
		Module *module = owner.get();

		codegen(module);

		if (debug)
			errs() << *module;

		EngineBuilder EB(std::move(owner));
		EB.setMCJITMemoryManager(make_unique<SectionMemoryManager>());
		EB.setUseOrcMCJITReplacement(true);
		ExecutionEngine *eng = EB.create();

		if (!standalone)
			for (auto& pipeline : once.pipelines) {
				pipeline.getHead()->finalize(module, eng);
			}

		for (auto& pipeline : main.pipelines) {
			pipeline.getHead()->finalize(module, eng);
		}

		if (!standalone)
			for (auto& pipeline : last.pipelines) {
				pipeline.getHead()->finalize(module, eng);
			}

		if (standalone) {
			auto op = (SeqMainStandalone)eng->getPointerToFunction(func);
			auto numArgs = (seq_int_t)args.size();
			arr_t<str_t> argsArr = {numArgs, new str_t[numArgs]};

			for (seq_int_t i = 0; i < numArgs; i++)
				argsArr.arr[i] = {(seq_int_t)args[i].size(), (char *)args[i].data()};

			op(argsArr);
		} else {
			auto op = (SeqMain)eng->getPointerToFunction(func);
			data = new io::DataBlock();
			std::vector<std::ifstream *> ins;

			for (auto &source : sources) {
				ins.push_back(new std::ifstream(source));

				if (!ins.back()->good())
					throw exc::IOException("could not open '" + source + "' for reading");
			}

			do {
				data->read(ins, fmt);
				const size_t len = data->len;

				for (size_t i = 0; i < len; i++) {
					const bool isLast = data->last && i == len - 1;
					op(data->block[i].seqs.data(), isLast);
				}
			} while (data->len > 0);

			for (auto *in : ins) {
				in->close();
				delete in;
			}
		}
	} catch (std::exception& e) {
		errs() << e.what() << '\n';
		throw;
	}
}

void SeqModule::add(Pipeline pipeline)
{
	main.add(pipeline);
}

Pipeline SeqModule::operator|(Pipeline to)
{
	return main | to;
}

Pipeline SeqModule::operator|(PipelineList& to)
{
	return main | to;
}

Pipeline SeqModule::operator|(Var& to)
{
	return main | to;
}

Pipeline SeqModule::operator&(PipelineList& to)
{
	return main & to;
}

Pipeline SeqModule::operator||(Pipeline to)
{
	return main || to;
}

Pipeline SeqModule::operator&&(PipelineList& to)
{
	return main && to;
}

PipelineAggregatorProxy SeqModule::operator[](unsigned idx)
{
	return {main, idx};
}
