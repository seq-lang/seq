#include "seq/seq.h"

using namespace seq;
using namespace llvm;

static void ensureNonVoid(types::Type *type)
{
	if (type->is(types::Void))
		throw exc::SeqException("cannot load or store void variable");
}

static int nameIdx = 0;

Var::Var(types::Type *type) :
    name("seq.var." + std::to_string(nameIdx++)), type(type), ptr(nullptr),
    module(nullptr), global(false), repl(false), mapped()
{
}

std::string Var::getName()
{
	return name;
}

void Var::setName(std::string name)
{
	this->name = std::move(name);
}

void Var::allocaIfNeeded(BaseFunc *base)
{
	if (!mapped.empty())
		mapped.top()->allocaIfNeeded(base);

	if (module != base->getPreamble()->getModule()) {
		ptr = nullptr;
		module = base->getPreamble()->getModule();
	}

	if (ptr)
		return;

	LLVMContext& context = base->getContext();
	if (global) {
		Type *llvmType = getType()->getLLVMType(context);

		if (repl)
			llvmType = llvmType->getPointerTo();

		ptr = new GlobalVariable(*module,
		                         llvmType,
		                         false,
		                         repl ? GlobalValue::ExternalLinkage : GlobalValue::PrivateLinkage,
		                         Constant::getNullValue(llvmType),
		                         name);
	} else {
		ptr = makeAlloca(getType()->getLLVMType(context), base->getPreamble());
	}
}

bool Var::isGlobal()
{
	if (!mapped.empty())
		return mapped.top()->isGlobal();
	return global;
}

void Var::setGlobal()
{
	if (!mapped.empty())
		mapped.top()->setGlobal();
	else
		global = true;
}

void Var::setREPL()
{
	if (!mapped.empty())
		mapped.top()->setGlobal();
	else {
		global = true;
		repl = true;
	}
}

void Var::mapTo(Var *other)
{
	mapped.push(other);
}

void Var::unmap()
{
	mapped.pop();
}

Value *Var::getPtr(BaseFunc *base)
{
	if (!mapped.empty())
		return mapped.top()->getPtr(base);

	allocaIfNeeded(base);
	assert(ptr);
	return ptr;
}

Value *Var::load(BaseFunc *base, BasicBlock *block, bool atomic)
{
	if (!mapped.empty())
		return mapped.top()->load(base, block);

	ensureNonVoid(getType());
	allocaIfNeeded(base);
	IRBuilder<> builder(block);
	auto *inst = builder.CreateLoad(ptr);
	Value *val = inst;

	if (repl) {
		inst = builder.CreateLoad(val);
		val = inst;
	}

	if (atomic)
		inst->setAtomic(AtomicOrdering::SequentiallyConsistent);

	return val;
}

void Var::store(BaseFunc *base, Value *val, BasicBlock *block, bool atomic)
{
	if (!mapped.empty()) {
		mapped.top()->store(base, val, block);
		return;
	}

	ensureNonVoid(getType());
	allocaIfNeeded(base);
	IRBuilder<> builder(block);
	Value *dest = repl ? builder.CreateLoad(ptr) : ptr;
	auto *inst = builder.CreateStore(val, dest);
	if (atomic)
		inst->setAtomic(AtomicOrdering::SequentiallyConsistent);
}

void Var::setType(types::Type *type)
{
	if (!mapped.empty())
		mapped.top()->setType(type);
	else
		this->type = type;
}

types::Type *Var::getType()
{
	if (!mapped.empty())
		return mapped.top()->getType();

	assert(type);
	return type;
}

Var *Var::clone(Generic *ref)
{
	if (isGlobal())
		return this;

	if (ref->seenClone(this))
		return (Var *)ref->getClone(this);

	// we intentionally don't clone this->mapped; should be set in codegen if needed
	auto *x = new Var();
	ref->addClone(this, x);
	if (type) x->setType(type->clone(ref));
	return x;
}
