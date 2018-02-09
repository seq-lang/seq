#include <iostream>
#include <string>
#include <vector>
#include "basestage.h"
#include "util.h"
#include "exc.h"
#include "seq.h"

using namespace seq;
using namespace llvm;

Seq::Seq() : src(""), func(nullptr)
{
}

void Seq::source(std::string source)
{
	src = std::move(source);
}

void Seq::codegen(Module *module, LLVMContext& context)
{
	func = cast<Function>(
	         module->getOrInsertFunction(
	           "main",
	           Type::getVoidTy(context),
	           IntegerType::getInt8PtrTy(context),
	           IntegerType::getInt32Ty(context)));

	auto args = func->arg_begin();
	Value *seq = args++;
	Value *len = args;

	seq->setName("seq");
	len->setName("len");

	BasicBlock *block = BasicBlock::Create(context, "entry", func);
	IRBuilder<> builder(block);

	for(auto& pipeline : pipelines) {
		pipeline->validate();
		builder.SetInsertPoint(&func->getBasicBlockList().back());
		block = BasicBlock::Create(context, "pipeline", func);
		builder.CreateBr(block);

		BaseStage *begin;
		if ((begin = dynamic_cast<BaseStage *>(pipeline->getHead()))) {
			begin->setBase(pipeline->getHead()->getBase());
			begin->block = block;
			pipeline->getHead()->codegen(module, context);
		} else {
			begin = &BaseStage::make(types::Void::get(), types::Seq::get());
			begin->setBase(pipeline->getHead()->getBase());
			begin->block = block;
			begin->outs->insert({SeqData::SEQ, seq});
			begin->outs->insert({SeqData::LEN, len});
			pipeline = &(*begin | *pipeline);
			begin->codegen(module, context);
		}
	}

	builder.SetInsertPoint(&func->getBasicBlockList().back());
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

		codegen(M, context);

		if (debug)
			errs() << *M;

		EngineBuilder EB(std::move(owner));
		EB.setMCJITMemoryManager(make_unique<SectionMemoryManager>());
		EB.setUseOrcMCJITReplacement(true);
		ExecutionEngine *eng = EB.create();

		for (auto& pipeline : pipelines) {
			pipeline->getHead()->finalize(eng);
		}

		auto op = (SeqOp)eng->getPointerToFunction(func);

		auto *data = new io::DataBlock();
		std::ifstream input(src);

		if (!input.good())
			throw exc::IOException("could not open '" + src + "' for reading");

		do {
			data->read(input, fmt);
			const size_t len = data->len;
			for (size_t i = 0; i < len; i++) {
				op(data->block[i].data[SeqData::SEQ], data->block[i].lens[SeqData::SEQ]);
			}
		} while (data->len > 0);

		delete data;
	} catch (std::exception& e) {
		errs() << e.what() << '\n';
		throw;
	}
}

void Seq::add(Pipeline *pipeline)
{
	pipelines.push_back(pipeline);
}

Pipeline& Seq::operator|(Pipeline& to)
{
	to.getHead()->setBase(this);
	add(&to);
	return to;
}

Pipeline& Seq::operator|(Stage& to)
{
	return (*this | to.asPipeline());
}
