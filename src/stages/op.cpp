#include <string>
#include <vector>
#include "seq/exc.h"
#include "seq/op.h"

using namespace seq;
using namespace llvm;

Op::Op(std::string name, SeqOp op) :
    Stage(std::move(name), types::SeqType::get(), types::SeqType::get()), op(op)
{
}

void Op::codegen(Module *module)
{
	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();
	func = cast<Function>(
	         module->getOrInsertFunction(
	           name,
	           Type::getVoidTy(context),
	           IntegerType::getInt8PtrTy(context),
	           seqIntLLVM(context)));

	func->setCallingConv(CallingConv::C);

	block = prev->block;
	outs->insert(prev->outs->begin(), prev->outs->end());
	IRBuilder<> builder(block);
	Value *seq = builder.CreateLoad(getSafe(outs, SeqData::SEQ));
	Value *len = builder.CreateLoad(getSafe(outs, SeqData::LEN));
	std::vector<Value *> args = {seq, len};
	builder.CreateCall(func, args, "");

	codegenNext(module);
	prev->setAfter(getAfter());
}

void Op::finalize(Module *module, ExecutionEngine *eng)
{
	eng->addGlobalMapping(func, (void *)op);
	Stage::finalize(module, eng);
}

Op& Op::make(std::string name, SeqOp op)
{
	return *new Op(std::move(name), op);
}
