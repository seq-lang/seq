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

	block = prev->block;
	IRBuilder<> builder(block);

	GlobalVariable *ptrVar = new GlobalVariable(*module,
	                                            PointerType::get(type->getBaseType()->getLLVMType(context), 0),
	                                            false,
	                                            GlobalValue::PrivateLinkage,
	                                            nullptr,
	                                            "mem");
	ptrVar->setInitializer(
	  ConstantPointerNull::get(PointerType::get(type->getBaseType()->getLLVMType(context), 0)));

	GlobalVariable *lenVar = new GlobalVariable(*module,
	                                            seqIntLLVM(context),
	                                            false,
	                                            GlobalValue::PrivateLinkage,
	                                            nullptr,
	                                            "len");
	lenVar->setInitializer(zeroLLVM(context));

	Value *ptr = type->getBaseType()->codegenAlloc(getBase(), count, block);
	Value *len = ConstantInt::get(seqIntLLVM(context), (uint64_t)count);
	builder.CreateStore(ptr, ptrVar);
	builder.CreateStore(len, lenVar);

	outs->insert({SeqData::ARRAY, ptrVar});
	outs->insert({SeqData::LEN, lenVar});

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
    Stage("loadstore", types::VoidType::get(), types::VoidType::get()),
    ptr(ptr), idx(idx), constIdx(-1), isStore(false)
{
	ptr->ensureConsistentBase(idx->getBase());
	setBase(ptr->getBase());
}

LoadStore::LoadStore(Var *ptr, seq_int_t idx) :
    Stage("loadstore", types::VoidType::get(), types::VoidType::get()),
    ptr(ptr), idx(nullptr), constIdx(idx), isStore(false)
{
	setBase(ptr->getBase());
}

void LoadStore::validate()
{
	types::Type *type = ptr->getType(this);

	if (idx && !idx->getType(this)->isChildOf(types::IntType::get()))
		throw exc::SeqException("non-integer array index");

	// somewhat contrived logic for determining whether we are loading or storing...
	const bool noPrev = (!getPrev() || getPrev()->getOutType()->isChildOf(types::VoidType::get()));
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

	block = prev->block;
	IRBuilder<> builder(block);
	Value *ptrVal = builder.CreateLoad(getSafe(ptr->outs(this), type->getKey()));
	Value *idxVal = idx ? (Value *)builder.CreateLoad(getSafe(idx->outs(this), SeqData::INT)) :
	                      (Value *)ConstantInt::get(seqIntLLVM(context), (uint64_t)constIdx);

	if (isStore) {
		type->codegenIndexStore(getBase(),
		                        prev->outs,
		                        block,
		                        ptrVal,
		                        idxVal);
	} else {
		type->codegenIndexLoad(getBase(),
		                       outs,
		                       block,
		                       ptrVal,
		                       idxVal);
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
