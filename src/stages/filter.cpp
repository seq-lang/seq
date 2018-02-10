#include <string>
#include <vector>
#include "exc.h"
#include "filter.h"

using namespace seq;
using namespace llvm;

Filter::Filter(std::string name, SeqPred op) :
    Stage(std::move(name), types::Seq::get(), types::Seq::get()), op(op)
{
}

void Filter::codegen(Module *module, LLVMContext& context)
{
	ensurePrev();
	validate();

	func = cast<Function>(
	         module->getOrInsertFunction(
		       name,
	           Type::getInt8Ty(context),
	           IntegerType::getInt8PtrTy(context),
	           IntegerType::getInt32Ty(context)));

	func->setCallingConv(CallingConv::C);

	block = prev->block;
	outs->insert(prev->outs->begin(), prev->outs->end());

	auto seqiter = outs->find(SeqData::SEQ);
	auto leniter = outs->find(SeqData::LEN);

	if (seqiter == outs->end() || leniter == outs->end())
		throw exc::StageException("pipeline error", *this);

	std::vector<Value *> args = {seqiter->second, leniter->second};
	IRBuilder<> builder(block);
	Value *pred = builder.CreateCall(func, args, "");

	BasicBlock *body = BasicBlock::Create(context, "body", block->getParent());
	block = body;

	codegenNext(module, context);

	BasicBlock *exit = BasicBlock::Create(context, "exit", body->getParent());
	builder.CreateCondBr(pred, body, exit);
	builder.SetInsertPoint(getAfter());
	builder.CreateBr(exit);
	prev->setAfter(exit);
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
