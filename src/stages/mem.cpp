#include <cstdlib>
#include <vector>
#include <cassert>
#include "basestage.h"
#include "seq.h"
#include "var.h"
#include "mem.h"

using namespace seq;
using namespace llvm;

Mem::Mem(types::Type *type, seq_int_t count) :
    Stage("mem", types::AnyType::get(), types::ArrayType::get(type)), count(count)
{
	name += "(" + type->getName() + "," + std::to_string(count) + ")";
}

void Mem::codegen(llvm::Module *module)
{
	ensurePrev();
	validate();

	auto *type = dynamic_cast<types::ArrayType *>(getOutType());
	assert(type != nullptr);
	block = prev->block;
	type->getBaseType()->callAlloc(getBase(), outs, count, block);
	codegenNext(module);
	prev->setAfter(getAfter());
}

void Mem::finalize(ExecutionEngine *eng)
{
	auto *type = dynamic_cast<types::ArrayType *>(getOutType());
	assert(type != nullptr);
	type->getBaseType()->finalizeAlloc(eng);
	Stage::finalize(eng);
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

	if (!type->isChildOf(types::ArrayType::get()))
		throw exc::SeqException("cannot index into non-array type '" + type->getName() + "'");

	if (!idx->getType(this)->isChildOf(types::IntType::get()))
		throw exc::SeqException("non-integer array index");

	auto *arrayType = dynamic_cast<types::ArrayType *>(type);
	assert(type != nullptr);

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

void LoadStore::codegen(Module *module)
{
	validate();

	auto *arrayType = dynamic_cast<types::ArrayType *>(ptr->getType(this));
	assert(arrayType != nullptr);

	auto ptriter = ptr->outs(this)->find(SeqData::ARRAY);

	if (ptriter == ptr->outs(this)->end())
		throw exc::StageException("pipeline error", *this);

	block = prev->block;
	IRBuilder<> builder(block);

	auto idxiter = idx->outs(this)->find(SeqData::INT);

	if (idxiter == idx->outs(this)->end())
		throw exc::StageException("pipeline error", *this);

	Value *ptr = builder.CreateLoad(ptriter->second);
	Value *idx = idxiter->second;

	if (isStore) {
		arrayType->getBaseType()->codegenStore(getBase(),
		                                       prev->outs,
		                                       block,
		                                       ptr,
		                                       idx);
	} else {
		arrayType->getBaseType()->codegenLoad(getBase(),
		                                      outs,
		                                      block,
		                                      ptr,
		                                      idx);
	}

	codegenNext(module);
	prev->setAfter(getAfter());
}

Pipeline LoadStore::operator|(Pipeline to)
{
	Pipeline p = Stage::operator|(to);

	if (!p.isAdded()) {
		Seq *base = getBase();
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
