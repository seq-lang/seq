#include <string>
#include <vector>
#include "exc.h"
#include "hash.h"

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
	func = cast<Function>(
	         module->getOrInsertFunction(
	           name,
	           seqIntLLVM(context),
	           IntegerType::getInt8PtrTy(context),
	           seqIntLLVM(context)));

	func->setCallingConv(CallingConv::C);

	block = prev->block;
	IRBuilder<> builder(block);
	std::vector<Value *> args = {getSafe(prev->outs, SeqData::SEQ), getSafe(prev->outs, SeqData::LEN)};
	Value *hashVal = builder.CreateCall(func, args, "");

	outs->insert({SeqData::INT, hashVal});

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
