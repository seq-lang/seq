#include <cstdlib>
#include <vector>
#include <cassert>
#include "seq/basestage.h"
#include "seq/seq.h"
#include "seq/var.h"
#include "seq/mem.h"

using namespace seq;
using namespace llvm;

Mem::Mem(types::Type *type, seq_int_t count) :
    Stage("mem", types::AnyType::get(), types::ArrayType::get(type)), count(count)
{
	name += "(" + type->getName() + "," + std::to_string(count) + ")";
}

void Mem::codegen(Module *module)
{
	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();
	auto *type = dynamic_cast<types::ArrayType *>(getOutType());
	assert(type != nullptr);

	block = prev->getAfter();
	Value *ptr = type->getBaseType()->alloc(count, block);
	Value *len = ConstantInt::get(seqIntLLVM(context), (uint64_t)count);
	Value *arr = type->make(ptr, len, block);
	result = getOutType()->storeInAlloca(getBase(), arr, block, true);

	codegenNext(module);
	prev->setAfter(getAfter());
}

void Mem::finalize(Module *module, ExecutionEngine *eng)
{
	auto *type = dynamic_cast<types::ArrayType *>(getOutType());
	assert(type != nullptr);
	type->getBaseType()->finalizeAlloc(module, eng);
	Stage::finalize(module, eng);
}

Mem& Mem::make(types::Type *type, seq_int_t count)
{
	return *new Mem(type, count);
}

LoadStore::LoadStore(Var *ptr, Var *idx) :
    Stage("loadstore"), ptr(ptr), idx(idx), constIdx(-1), isStore(false)
{
	ptr->ensureConsistentBase(idx->getBase());
	setBase(ptr->getBase());
}

LoadStore::LoadStore(Var *ptr, seq_int_t idx) :
    Stage("loadstore"), ptr(ptr), idx(nullptr), constIdx(idx), isStore(false)
{
	setBase(ptr->getBase());
}

void LoadStore::validate()
{
	types::Type *type = ptr->getType(this);

	if (idx && !idx->getType(this)->isChildOf(types::IntType::get()))
		throw exc::SeqException("non-integer array index");

	// somewhat contrived logic for determining whether we are loading or storing...
	const bool noPrev = (!getPrev() || getPrev()->getOutType()->is(types::VoidType::get()));
	const bool noNext = (getNext().empty() && getWeakNext().empty());

	if (noPrev && noNext)
		isStore = false;
	else
		isStore = noNext;

	if (isStore) {
		in = type->getBaseType(constIdx);  // might not be a const load/store, but then this call should fail
		out = types::VoidType::get();
	} else {
		in = types::AnyType::get();
		out = type->getBaseType(constIdx);
	}

	Stage::validate();
}

void LoadStore::codegen(Module *module)
{
	validate();

	LLVMContext& context = module->getContext();
	types::Type *type = ptr->getType(this);

	block = prev->getAfter();
	IRBuilder<> builder(block);
	Value *arr = builder.CreateLoad(ptr->result(this));
	Value *idx = this->idx ? (Value *)builder.CreateLoad(this->idx->result(this)) :
	                         (Value *)ConstantInt::get(seqIntLLVM(context), (uint64_t)constIdx);

	if (isStore) {
		Value *val = builder.CreateLoad(prev->result);
		type->indexStore(getBase(), arr, idx, val, block);
	} else {
		Value *val = type->indexLoad(getBase(), arr, idx, block);
		result = getOutType()->storeInAlloca(getBase(), val, block, true);
	}

	codegenNext(module);
	prev->setAfter(getAfter());
}

Pipeline LoadStore::operator|(Pipeline to)
{
	Pipeline p = Stage::operator|(to);

	if (!p.isAdded()) {
		BaseFunc *base = getBase();
		p.getHead()->setBase(base);
		BaseStage& begin = BaseStage::make(types::VoidType::get(), types::VoidType::get(), this);
		begin.setBase(base);
		Pipeline full = begin | p;
		base->add(full);
	}

	return p;
}

LoadStore& LoadStore::make(Var *ptr, Var *idx)
{
	return *new LoadStore(ptr, idx);
}

LoadStore& LoadStore::make(Var *ptr, seq_int_t idx)
{
	return *new LoadStore(ptr, idx);
}
