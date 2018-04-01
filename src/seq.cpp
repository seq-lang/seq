#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include "common.h"
#include "basestage.h"
#include "util.h"
#include "exc.h"
#include "seq.h"

using namespace seq;
using namespace llvm;

PipelineAggregator::PipelineAggregator(Seq *base) : base(base), pipelines()
{
}

void PipelineAggregator::add(Pipeline pipeline)
{
	if (pipeline.isAdded())
		throw exc::MultiLinkException(*pipeline.getHead());

	pipelines.push_back(pipeline);
	pipeline.setAdded();
}

Pipeline PipelineAggregator::addWithIndex(Pipeline to, seq_int_t idx)
{
	idx -= 1;  // 1-based to 0-based
	if (idx < 0 || idx >= io::MAX_INPUTS)
		throw exc::SeqException("invalid sequence index specified");

	to.getHead()->setBase(base);
	BaseStage& begin = BaseStage::make(types::VoidType::get(), types::SeqType::get());
	begin.setBase(base);
	begin.outs = base->outs[idx];
	Pipeline full = begin | to;
	add(full);

	return full;
}

Pipeline PipelineAggregator::operator|(Pipeline to)
{
	return addWithIndex(to, 1);
}

Pipeline PipelineAggregator::operator|(PipelineList to)
{
	for (auto *node = to.head; node; node = node->next) {
		*this | node->p;
	}

	return {to.head->p.getHead(), to.tail->p.getTail()};
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

Pipeline PipelineAggregatorProxy::operator|(Pipeline to)
{
	return aggr.addWithIndex(to, idx);
}

Pipeline PipelineAggregatorProxy::operator|(PipelineList to)
{
	for (auto *node = to.head; node; node = node->next) {
		*this | node->p;
	}

	return {to.head->p.getHead(), to.tail->p.getTail()};
}

Pipeline PipelineAggregatorProxy::operator|(Var& to)
{
	return aggr | to;
}

PipelineAggregatorProxy::PipelineAggregatorProxy(PipelineAggregator& aggr, seq_int_t idx) :
    aggr(aggr), idx(idx)
{
}

PipelineAggregatorProxy::PipelineAggregatorProxy(PipelineAggregator& aggr) :
    PipelineAggregatorProxy(aggr, 1)
{
}


Seq::Seq() :
    BaseFunc(), sources(), main(this), once(this), last(this)
{
	for (auto& out : outs)
		out = std::make_shared<std::map<SeqData, Value *>>(*new std::map<SeqData, Value *>());
}

void Seq::source(std::string source)
{
	sources.push_back(source);
}

void Seq::codegen(Module *module)
{
	if (func)
		return;

	compilationContext.reset();
	LLVMContext& context = module->getContext();
	this->module = module;

	func = cast<Function>(
	         module->getOrInsertFunction(
	           "main",
	           Type::getVoidTy(context),
	           PointerType::get(types::Seq.getLLVMType(context), 0),
	           IntegerType::getInt8Ty(context)));

	auto args = func->arg_begin();
	Value *seqs = args++;
	Value *isLast = args;
	isLast->setName("last");

	/* preamble */
	preambleBlock = BasicBlock::Create(context, "preamble", func);
	IRBuilder<> builder(preambleBlock);

	codegenInit(module);
	builder.CreateCall(initFunc);

	for (size_t i = 0; i < sources.size(); i++) {
		types::Seq.codegenLoad(this,
		                       outs[i],
		                       preambleBlock,
		                       seqs,
		                       ConstantInt::get(seqIntLLVM(context), i));
	}

	/* one-time execution */
	BasicBlock *onceBr = BasicBlock::Create(context, "oncebr", func);
	BasicBlock *origOnceBlock = BasicBlock::Create(context, "once", func);
	BasicBlock *onceBlock = origOnceBlock;  // onceBlock is really the _last_ once-block
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

	GlobalVariable *init = new GlobalVariable(*module,
	                                          IntegerType::getInt1Ty(context),
	                                          false,
	                                          GlobalValue::PrivateLinkage,
	                                          nullptr,
	                                          "init");

	init->setInitializer(ConstantInt::get(IntegerType::getInt1Ty(context), 0));

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

	BasicBlock *lastMain = &func->getBasicBlockList().back();

	/* last */
	BasicBlock *lastBr = BasicBlock::Create(context, "lastbr", func);
	BasicBlock *origLastBlock = BasicBlock::Create(context, "last", func);
	BasicBlock *lastBlock = origLastBlock;  // lastBlock is really the _last_ last-block

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

	lastBlock = &func->getBasicBlockList().back();
	BasicBlock *exit = BasicBlock::Create(context, "exit", func);

	/* stitch it all together */
	builder.SetInsertPoint(preambleBlock);
	builder.CreateBr(onceBr);

	builder.SetInsertPoint(onceBr);
	builder.CreateCondBr(builder.CreateLoad(init), entry, origOnceBlock);

	builder.SetInsertPoint(onceBlock);
	builder.CreateStore(ConstantInt::get(IntegerType::getInt1Ty(context), 1), init);
	builder.CreateBr(entry);

	builder.SetInsertPoint(lastMain);
	builder.CreateBr(lastBr);

	builder.SetInsertPoint(lastBr);
	builder.CreateCondBr(isLast, origLastBlock, exit);

	builder.SetInsertPoint(lastBlock);
	builder.CreateBr(exit);

	builder.SetInsertPoint(exit);
	builder.CreateRetVoid();

	finalizeInit(module);
}

void Seq::codegenCall(BaseFunc *base, ValMap ins, ValMap outs, BasicBlock *block)
{
	throw exc::SeqException("cannot call Seq instance");
}

static io::Format extractExt(const std::string& source)
{
	auto fmtIter = io::EXT_CONV.find(source.substr(source.find_last_of('.') + 1));

	if (fmtIter == io::EXT_CONV.end())
		throw exc::IOException("unknown file extension in '" + source + "'");

	return fmtIter->second;
}

void Seq::execute(bool debug)
{
	try {
		if (sources.empty())
			throw exc::SeqException("sequence source not specified");

		if (sources.size() > io::MAX_INPUTS)
			throw exc::SeqException("too many inputs (max: " + std::to_string(io::MAX_INPUTS) + ")");

		io::Format fmt = extractExt(sources[0]);

		for (const auto& src : sources) {
			if (extractExt(src) != fmt)
				throw exc::SeqException("inconsistent input formats");
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

		for (auto& pipeline : once.pipelines) {
			pipeline.getHead()->finalize(eng);
		}

		for (auto& pipeline : main.pipelines) {
			pipeline.getHead()->finalize(eng);
		}

		for (auto& pipeline : last.pipelines) {
			pipeline.getHead()->finalize(eng);
		}

		auto op = (SeqMain)eng->getPointerToFunction(func);
		auto *data = new io::DataBlock();
		std::vector<std::ifstream *> ins;

		for (auto& source : sources) {
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

		delete data;
	} catch (std::exception& e) {
		errs() << e.what() << '\n';
		throw;
	}
}

void Seq::add(Pipeline pipeline)
{
	main.add(pipeline);
}

Pipeline Seq::operator|(Pipeline to)
{
	return main | to;
}

Pipeline Seq::operator|(PipelineList to)
{
	return main | to;
}

Pipeline Seq::operator|(Var& to)
{
	return main | to;
}

PipelineAggregatorProxy Seq::operator[](unsigned idx)
{
	return {main, idx};
}
