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

	func = cast<Function>(
	         module->getOrInsertFunction(
	           name,
	           seqIntLLVM(context),
	           IntegerType::getInt8PtrTy(context),
	           seqIntLLVM(context)));

	func->setCallingConv(CallingConv::C);

	block = prev->getAfter();
	IRBuilder<> builder(block);
	Value *seq = builder.CreateLoad(prev->result);
	Value *ptr = types::Seq.memb(seq, "ptr", block);
	Value *len = types::Seq.memb(seq, "len", block);

	Value *hash = builder.CreateCall(func, {ptr, len});
	result = types::Int.storeInAlloca(getBase(), hash, block, true);

	codegenNext(module);
	prev->setAfter(getAfter());
}

void Hash::finalize(Module *module, ExecutionEngine *eng)
{
	eng->addGlobalMapping(func, (void *)hash);
	Stage::finalize(module, eng);
}

Hash& Hash::make(std::string name, SeqHash hash)
{
	return *new Hash(std::move(name), hash);
}

Hash *Hash::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (Hash *)ref->getClone(this);

	Hash& x = Hash::make(name, hash);
	ref->addClone(this, &x);
	Stage::setCloneBase(&x, ref);
	return &x;
}
