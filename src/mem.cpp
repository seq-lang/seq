#include <cstdlib>
#include <vector>
#include "basestage.h"
#include "seq.h"
#include "var.h"
#include "mem.h"

using namespace seq;
using namespace llvm;

Mem::Mem(types::Type *of, const uint32_t size, Seq *base) :
    base(base), of(of), size(size), mallocFunc(nullptr), ptr(nullptr)
{
}

types::Type *Mem::getType() const
{
	return of;
}

Seq *Mem::getBase() const
{
	return base;
}

void Mem::makeMalloc(Module *module, LLVMContext& context)
{
	if (mallocFunc)
		return;

	mallocFunc = cast<Function>(
	               module->getOrInsertFunction(
	                 "malloc",
	                 IntegerType::getInt8PtrTy(context),
	                 IntegerType::getIntNTy(context, sizeof(size_t)*8)));
}

void Mem::finalizeMalloc(ExecutionEngine *eng)
{
	eng->addGlobalMapping(mallocFunc, (void *)std::malloc);
}

void Mem::codegenAlloc(Module *module, LLVMContext& context)
{
	if (ptr)
		return;

	if (of->size() == 0)
		throw exc::SeqException("cannot create array of specified type");

	makeMalloc(module, context);
	BasicBlock *once = base->getOnce();
	IRBuilder<> builder(once);

	ptr = new GlobalVariable(*module,
	                         IntegerType::getIntNPtrTy(context, of->size()*8),
	                         false,
	                         GlobalValue::PrivateLinkage,
	                         nullptr,
	                         "mem");

	ptr->setInitializer(
	  ConstantPointerNull::get(IntegerType::getIntNPtrTy(context, of->size())));

	std::vector<Value *> args = {
	  ConstantInt::get(IntegerType::getIntNTy(context, sizeof(size_t)*8), size * of->size())};
	Value *mem = builder.CreateCall(mallocFunc, args);
	mem = builder.CreatePointerCast(mem, IntegerType::getIntNPtrTy(context, of->size()*8));
	builder.CreateStore(mem, ptr);
}

Value *Mem::codegenLoad(Module *module,
                        LLVMContext& context,
                        BasicBlock *block,
                        Value *idx)
{
	return of->codegenLoad(module, context, block, ptr, idx);
}

void Mem::codegenStore(Module *module,
                       LLVMContext& context,
                       BasicBlock *block,
                       Value *idx,
                       Value *val)
{
	of->codegenStore(module, context, block, ptr, idx, val);
}

LoadStore& Mem::operator[](Var& idx)
{
	LoadStore& ls = *new LoadStore(this, &idx);
	return ls;
}

LoadStore::LoadStore(Mem *mem, Var *idx) :
    Stage("loadstore", mem->getType(), mem->getType()),
    mem(mem), idx(idx), isStore(false)
{
	setBase(mem->getBase());
}

void LoadStore::validate()
{
	if (idx->getType() != types::Int::get())
		throw exc::SeqException("non-integer array index");

	isStore = getPrev() && (getPrev()->getOutType() != types::Void::get());

	if (isStore) {
		in = mem->getType();
		out = types::Void::get();
	} else {
		in = types::Void::get();
		out = mem->getType();
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
	mem->codegenAlloc(module, context);

	block = prev->block;
	IRBuilder<> builder(block);

	if (isStore) {
		SeqData key = prev->getOutType()->getKey();
		ensureKey(key);

		auto idxiter = idx->outs()->find(SeqData::INT);
		auto valiter = prev->outs->find(key);

		if (idxiter == idx->outs()->end() || valiter == prev->outs->end())
			throw exc::StageException("pipeline error", *this);

		mem->codegenStore(module, context, block, idxiter->second, valiter->second);
	} else {
		SeqData key = mem->getType()->getKey();
		ensureKey(key);

		auto idxiter = idx->outs()->find(SeqData::INT);

		if (idxiter == idx->outs()->end())
			throw exc::StageException("pipeline error", *this);

		Value *val = mem->codegenLoad(module, context, block, idxiter->second);

		outs->insert({key, val});
	}

	codegenNext(module, context);
	prev->setAfter(getAfter());
}

void LoadStore::finalize(ExecutionEngine *eng)
{
	mem->finalizeMalloc(eng);
}

static void addWithBase(Pipeline& pipeline, Seq *base)
{
	pipeline.getHead()->setBase(base);
	BaseStage& begin = BaseStage::make(types::Void::get(), types::Void::get());
	begin.setBase(base);
	Pipeline& full = begin | pipeline;
	base->add(&full);
}

Pipeline& LoadStore::operator|(Stage& to)
{
	Pipeline& full = Stage::operator|(to);

	if (!full.isAdded())
		addWithBase(full, mem->getBase());

	return full;
}

Pipeline& LoadStore::operator|(Pipeline& to)
{
	Pipeline& full = Stage::operator|(to);

	if (!full.isAdded())
		addWithBase(full, mem->getBase());

	return full;
}
