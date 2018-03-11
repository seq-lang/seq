#include <string>
#include <vector>
#include "exc.h"
#include "hash.h"

using namespace seq;
using namespace llvm;

Hash::Hash(std::string name, SeqHash hash) :
    Stage(std::move(name), types::Seq::get(), types::Int::get()), hash(hash)
{
}

void Hash::codegen(Module *module, LLVMContext& context)
{
	ensurePrev();
	validate();

	func = cast<Function>(
	         module->getOrInsertFunction(
	           name,
	           IntegerType::getInt32Ty(context),
	           IntegerType::getInt8PtrTy(context),
	           IntegerType::getInt32Ty(context)));

	func->setCallingConv(CallingConv::C);

	block = prev->block;

	auto seqiter = prev->outs->find(SeqData::SEQ);
	auto leniter = prev->outs->find(SeqData::LEN);

	if (seqiter == prev->outs->end() || leniter == prev->outs->end())
		throw exc::StageException("pipeline error", *this);

	IRBuilder<> builder(block);
	std::vector<Value *> args = {seqiter->second, leniter->second};
	Value *hashVal = builder.CreateCall(func, args, "");

	outs->insert({SeqData::INT, hashVal});

	codegenNext(module, context);
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
