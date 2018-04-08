#include "seq/seq.h"
#include "seq/exc.h"
#include "seq/copy.h"

using namespace seq;
using namespace llvm;

Copy::Copy() :
    Stage("copy", types::SeqType::get(), types::SeqType::get()), copyFunc(nullptr)
{
}

void Copy::codegen(Module *module)
{
	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();
	block = prev->block;

	IRBuilder<> builder(block);
	BasicBlock *preambleBlock = getBase()->getPreamble();

	Value *seq = builder.CreateLoad(getSafe(prev->outs, SeqData::SEQ));
	Value *len = builder.CreateLoad(getSafe(prev->outs, SeqData::LEN));

	if (!copyFunc) {
		copyFunc = cast<Function>(
		             module->getOrInsertFunction(
		               "copy",
		               IntegerType::getInt8PtrTy(context),
		               IntegerType::getInt8PtrTy(context),
		               seqIntLLVM(context)));
	}

	std::vector<Value *> args = {seq, len};
	Value *copy = builder.CreateCall(copyFunc, args);

	Value *copyVar = makeAlloca(nullPtrLLVM(context), preambleBlock);
	Value *lenVar = makeAlloca(zeroLLVM(context), preambleBlock);
	builder.CreateStore(copy, copyVar);
	builder.CreateStore(len, lenVar);

	outs->insert({SeqData::SEQ, copyVar});
	outs->insert({SeqData::LEN, lenVar});

	codegenNext(module);
	prev->setAfter(getAfter());
}

void Copy::finalize(Module *module, ExecutionEngine *eng)
{
	eng->addGlobalMapping(copyFunc, (void *)util::copy);
}

Copy& Copy::make()
{
	return *new Copy();
}
