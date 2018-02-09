#include <string>
#include <vector>
#include "exc.h"
#include "op.h"

using namespace seq;
using namespace llvm;

Op::Op(std::string name, SeqOp op) :
    Stage(std::move(name), types::Seq::get(), types::Seq::get()), op(op)
{
}

void Op::codegen(Module *module, LLVMContext& context)
{
	validate();

	if (!prev || !prev->block)
		throw exc::StageException("previous stage not compiled", *this);

	func = cast<Function>(
	         module->getOrInsertFunction(
               name,
	           Type::getVoidTy(context),
	           IntegerType::getInt8PtrTy(context),
	           IntegerType::getInt32Ty(context)));

	func->setCallingConv(CallingConv::C);

	block = prev->block;
	outs->insert(prev->outs->begin(), prev->outs->end());

	auto seqiter = outs->find(SeqData::SEQ);
	auto leniter = outs->find(SeqData::LEN);

	if (seqiter == outs->end() || leniter == outs->end())
		throw exc::StageException("pipeline error", *this);

	IRBuilder<> builder(block);
	std::vector<Value *> args = {seqiter->second, leniter->second};
	builder.CreateCall(func, args, "");

	codegenNext(module, context);
	prev->setAfter(getAfter());
}

void Op::finalize(ExecutionEngine *eng)
{
	eng->addGlobalMapping(func, (void *)op);
	Stage::finalize(eng);
}

Op& Op::make(std::string name, SeqOp op)
{
	return *new Op(std::move(name), op);
}
