#include <cstdlib>
#include <vector>
#include "basestage.h"
#include "seq.h"
#include "var.h"
#include "mem.h"

using namespace seq;
using namespace llvm;

Mem::Mem(types::Type *type, seq_int_t count) :
    Stage("mem", types::AnyType::get(), types::ArrayType::get(type, count))
{
}

void Mem::codegen(llvm::Module *module, llvm::LLVMContext &context)
{
	ensurePrev();
	validate();

	auto *type = (types::ArrayType *)getOutType();
	block = prev->block;
	type->callAlloc(outs, block);
	codegenNext(module, context);
	prev->setAfter(getAfter());
}

void Mem::finalize(ExecutionEngine *eng)
{
	auto *type = (types::ArrayType *)getOutType();
	type->finalizeAlloc(eng);
}

Mem& Mem::make(types::Type *type, seq_int_t count)
{
	return *new Mem(type, count);
}

LoadStore::LoadStore(Var *ptr, Var *idx) :
    Stage("loadstore", types::VoidType::get(), types::VoidType::get()),
    ptr(ptr), idx(idx), isStore(false)
{
	setBase(ptr->getBase());
}

void LoadStore::validate()
{
	types::Type *type = ptr->getType(this);

	if (!type->isChildOf(types::ArrayType::get(nullptr, 0)))
		throw exc::SeqException("cannot index into non-array type '" + type->getName() + "'");

	if (!idx->getType(this)->isChildOf(types::IntType::get()))
		throw exc::SeqException("non-integer array index");

	auto *arrayType = (types::ArrayType *)type;

	// somewhat contrived logic for determining whether we are loading or storing...
	const bool noPrev = (!getPrev() || getPrev()->getOutType()->isChildOf(types::VoidType::get()));
	const bool noNext = (getNext().empty() && getWeakNext().empty());

	if (noPrev && noNext)
		isStore = false;
	else
		isStore = noNext;

	if (isStore) {
		in = arrayType->getBaseType();
		out = types::VoidType::get();
	} else {
		in = types::AnyType::get();
		out = arrayType->getBaseType();
	}

	Stage::validate();
}

static void ensureKey(SeqData key)
{
	if (key == SeqData::NONE)
		throw exc::SeqException("unsupported array type");
}

void LoadStore::codegen(Module *module, LLVMContext& context)
{
	validate();

	auto *arrayType = (types::ArrayType *)ptr->getType(this);

	auto ptriter = ptr->outs(this)->find(SeqData::ARRAY);

	if (ptriter == ptr->outs(this)->end())
		throw exc::StageException("pipeline error", *this);

	block = prev->block;
	IRBuilder<> builder(block);

	if (isStore) {
		SeqData key = prev->getOutType()->getKey();
		ensureKey(key);

		auto idxiter = idx->outs(this)->find(SeqData::INT);
		auto valiter = prev->outs->find(key);

		if (idxiter == idx->outs(this)->end() || valiter == prev->outs->end())
			throw exc::StageException("pipeline error", *this);

		arrayType->codegenStore(block, ptriter->second, idxiter->second, valiter->second);
	} else {
		SeqData key = arrayType->getBaseType()->getKey();
		ensureKey(key);

		auto idxiter = idx->outs(this)->find(SeqData::INT);

		if (idxiter == idx->outs(this)->end())
			throw exc::StageException("pipeline error", *this);

		Value *val = arrayType->codegenLoad(block, ptriter->second, idxiter->second);

		outs->insert({key, val});
	}

	codegenNext(module, context);
	prev->setAfter(getAfter());
}

Pipeline LoadStore::operator|(Pipeline to)
{
	Pipeline p = Stage::operator|(to);

	if (!p.isAdded()) {
		Seq *base = getBase();
		p.getHead()->setBase(base);
		BaseStage& begin = BaseStage::make(types::VoidType::get(), types::VoidType::get());
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
