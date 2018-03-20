#include <iostream>
#include <string>
#include <vector>
#include <cassert>
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

Pipeline PipelineAggregator::operator|(Pipeline to)
{
	to.getHead()->setBase(base);
	BaseStage& begin = BaseStage::make(types::VoidType::get(), types::SeqType::get());
	begin.setBase(base);
	begin.outs = base->outs;
	Pipeline full = begin | to;
	add(full);

	return full;
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

	Stage *stage = to.getStage();
	BaseStage& begin = BaseStage::make(types::VoidType::get(), to.getType(stage), stage);
	begin.setBase(base);
	begin.outs = to.outs(stage);
	add(begin);

	return begin;
}

Seq::Seq() :
    src(""), pipelines(), outs(new std::map<SeqData, Value *>()),
    func(nullptr), preambleBlock(nullptr),
    main(this), once(this), last(this)
{
}

void Seq::source(std::string source)
{
	src = std::move(source);
}

void Seq::codegen(Module *module)
{
	LLVMContext& context = module->getContext();

	func = cast<Function>(
	         module->getOrInsertFunction(
	           "main",
	           Type::getVoidTy(context),
	           IntegerType::getInt8PtrTy(context),
	           seqIntLLVM(context),
	           IntegerType::getInt8Ty(context)));

	auto args = func->arg_begin();
	Value *seq = args++;
	Value *len = args++;
	Value *isLast = args;

	seq->setName("seq");
	len->setName("len");
	isLast->setName("last");

	outs->insert({SeqData::SEQ, seq});
	outs->insert({SeqData::LEN, len});

	/* preamble */
	preambleBlock = BasicBlock::Create(context, "preamble", func);

	/* one-time execution */
	BasicBlock *onceBr = BasicBlock::Create(context, "oncebr", func);
	BasicBlock *origOnceBlock = BasicBlock::Create(context, "once", func);
	BasicBlock *onceBlock = origOnceBlock;  // onceBlock is really the _last_ once-block
	IRBuilder<> builder(onceBlock);

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

	BasicBlock *lastMain = &func->getBasicBlockList().back();

	/* last */
	BasicBlock *lastBr = BasicBlock::Create(context, "lastbr", func);
	BasicBlock *origLastBlock = BasicBlock::Create(context, "last", func);
	BasicBlock *lastBlock = origLastBlock;  // lastBlock is really the _last_ last-block

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
}

void Seq::execute(bool debug)
{
	try {
		if (src.empty())
			throw exc::SeqException("sequence source not specified");

		auto fmtIter = io::EXT_CONV.find(src.substr(src.find_last_of('.') + 1));

		if (fmtIter == io::EXT_CONV.end())
			throw exc::IOException("unknown file extension in '" + src + "'");

		io::Format fmt = fmtIter->second;

		InitializeNativeTarget();
		InitializeNativeTargetAsmPrinter();
		LLVMContext context;

		std::unique_ptr<Module> owner(new Module("seq", context));
		Module *M = owner.get();

		codegen(M);

		if (debug)
			errs() << *M;

		EngineBuilder EB(std::move(owner));
		EB.setMCJITMemoryManager(make_unique<SectionMemoryManager>());
		EB.setUseOrcMCJITReplacement(true);
		ExecutionEngine *eng = EB.create();

		for (auto& pipeline : pipelines) {
			pipeline.getHead()->finalize(eng);
		}

		auto op = (SeqMain)eng->getPointerToFunction(func);

		auto *data = new io::DataBlock();
		std::ifstream input(src);

		if (!input.good())
			throw exc::IOException("could not open '" + src + "' for reading");

		do {
			data->read(input, fmt);
			const size_t len = data->len;
			for (size_t i = 0; i < len; i++) {
				op(data->block[i].data[SeqData::SEQ],
				   data->block[i].lens[SeqData::SEQ],
				   data->last && i == len - 1);
			}
		} while (data->len > 0);

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

BasicBlock *Seq::getPreamble() const
{
	if (!preambleBlock)
		throw exc::SeqException("cannot request preamble before code generation");

	return preambleBlock;
}

Pipeline Seq::operator|(Pipeline to)
{
	return main | to;
}

Pipeline Seq::operator|(PipelineList to)
{
	return main | to;
}
