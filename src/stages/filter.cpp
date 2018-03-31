#include <string>
#include <vector>
#include "exc.h"
#include "filter.h"

using namespace seq;
using namespace llvm;

Filter::Filter(std::string name, SeqPred op) :
    Stage(std::move(name), types::SeqType::get(), types::SeqType::get()), op(op)
{
}

void Filter::codegen(Module *module)
{
	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();
	func = cast<Function>(
	         module->getOrInsertFunction(
		       name,
	           Type::getInt8Ty(context),
	           IntegerType::getInt8PtrTy(context),
	           seqIntLLVM(context)));

	func->setCallingConv(CallingConv::C);

	block = prev->block;
	outs->insert(prev->outs->begin(), prev->outs->end());

	IRBuilder<> builder(block);
	Value *seq = builder.CreateLoad(getSafe(outs, SeqData::SEQ));
	Value *len = builder.CreateLoad(getSafe(outs, SeqData::LEN));
	std::vector<Value *> args = {seq, len};

	Value *pred = builder.CreateCall(func, args, "");

	BasicBlock *body = BasicBlock::Create(context, "body", block->getParent());
	block = body;

	codegenNext(module);

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
