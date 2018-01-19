#include <string>
#include <vector>
#include "../exc.h"
#include "filter.h"

using namespace seq;
using namespace llvm;

Filter::Filter(std::string name, SeqPred op) :
    Stage(std::move(name), types::Seq(), types::Seq()), op(op)
{
}

void Filter::codegen(Module *module, LLVMContext& context)
{
	validate();

	if (!prev || !prev->block)
		throw exc::StageException("previous stage not compiled", *this);

	func = cast<Function>(
			module->getOrInsertFunction(name,
			                            Type::getInt8Ty(context),
			                            IntegerType::getInt8PtrTy(context),
			                            IntegerType::getInt32Ty(context)));
	func->setCallingConv(CallingConv::C);

	block = prev->block;
	outs = prev->outs;

	auto seqiter = outs.find(SeqData::RESULT);
	auto leniter = outs.find(SeqData::LEN);

	if (seqiter == outs.end() || leniter == outs.end())
		throw exc::StageException("pipeline error", *this);

	std::vector<Value *> args = {seqiter->second, leniter->second};
	IRBuilder<> builder(block);
	Value *pred = builder.CreateCall(func, args, "");

	BasicBlock *body = BasicBlock::Create(context, "body", block->getParent());
	block = body;

	if (next)
		next->codegen(module, context);

	BasicBlock *after = BasicBlock::Create(context, "after", body->getParent());
	builder.CreateCondBr(pred, body, after);
	builder.SetInsertPoint(block);
	builder.CreateBr(after);
	prev->block = after;
}

void Filter::finalize(ExecutionEngine *eng)
{
	eng->addGlobalMapping(func, (void *)op);
	Stage::finalize(eng);
}

Filter& Filter::make(std::string name, SeqPred op)
{
	return *new Filter(std::move(name), op);
}
