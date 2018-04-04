#include <string>
#include <vector>
#include "seq/seq.h"
#include "seq/exc.h"
#include "seq/hash.h"

using namespace seq;
using namespace llvm;

Hash::Hash(std::string name, SeqHash hash) :
    Stage(std::move(name), types::SeqType::get(), types::IntType::get()), hash(hash)
{
}

void Hash::codegen(Module *module)
{
	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();
	BasicBlock *preambleBlock = getBase()->getPreamble();

	func = cast<Function>(
	         module->getOrInsertFunction(
	           name,
	           seqIntLLVM(context),
	           IntegerType::getInt8PtrTy(context),
	           seqIntLLVM(context)));

	func->setCallingConv(CallingConv::C);

	block = prev->block;
	IRBuilder<> builder(block);
	Value *seq = builder.CreateLoad(getSafe(prev->outs, SeqData::SEQ));
	Value *len = builder.CreateLoad(getSafe(prev->outs, SeqData::LEN));
	std::vector<Value *> args = {seq, len};

	Value *hashVar = makeAlloca(zeroLLVM(context), preambleBlock);
	builder.CreateStore(builder.CreateCall(func, args, ""), hashVar);

	outs->insert({SeqData::INT, hashVar});

	codegenNext(module);
	prev->setAfter(getAfter());
}

void Hash::finalize(ExecutionEngine *eng)
{
	eng->addGlobalMapping(func, (void *)hash);
	Stage::finalize(eng);
}

Hash& Hash::make(std::string name, SeqHash hash)
{
	return *new Hash(std::move(name), hash);
}
