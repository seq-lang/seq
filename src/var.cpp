#include "seq.h"
#include "stage.h"
#include "basestage.h"
#include "mem.h"
#include "exc.h"
#include "var.h"

using namespace seq;
using namespace llvm;

Var::Var() : stage(nullptr)
{
}

Var::Var(Pipeline pipeline) : Var()
{
	*this = pipeline;
}

types::Type *Var::getType(Stage *caller) const
{
	if (!isAssigned())
		throw exc::SeqException("variable used before assigned");

	return stage->getOutType();
}

ValMap Var::outs(Stage *caller) const
{
	if (!isAssigned())
		throw exc::SeqException("variable used before assigned");

	return stage->outs;
}

Stage *Var::getStage() const
{
	if (!isAssigned())
		throw exc::SeqException("variable used before assigned");

	return stage;
}

bool Var::isAssigned() const
{
	return stage != nullptr;
}

BaseFunc *Var::getBase() const
{
	if (!isAssigned())
		throw exc::SeqException("variable used before assigned");

	return stage->getBase();
}

Pipeline Var::operator|(Pipeline to)
{
	if (!isAssigned())
		throw exc::SeqException("variable used before assigned");

	if (to.isAdded())
		throw exc::SeqException("cannot use same pipeline twice");

	ensureConsistentBase(to.getHead()->getBase());
	BaseFunc *base = getBase();
	to.getHead()->setBase(base);
	BaseStage& begin = BaseStage::make(types::VoidType::get(), getType(stage), stage);
	begin.setBase(base);
	begin.outs = outs(stage);

	if (stage)
		stage->addWeakNext(to.getHead());

	Pipeline full = begin | to;
	base->add(full);
	return full;
}

Var& Var::operator=(Pipeline to)
{
	if (isAssigned())
		throw exc::SeqException("variable cannot be assigned twice");

	stage = to.getTail();
	BaseFunc *base = getBase();

	if (!to.isAdded()) {
		BaseStage& begin = BaseStage::make(types::VoidType::get(), types::VoidType::get());
		begin.setBase(base);
		Pipeline full = begin | to;
		base->add(full);
	}

	return *this;
}

LoadStore& Var::operator[](Var& idx)
{
	ensureConsistentBase(idx.getBase());
	return LoadStore::make(this, &idx);
}

void Var::ensureConsistentBase(BaseFunc *base)
{
	if (stage && stage->getBase() && base && stage->getBase() != base)
		throw exc::SeqException("cannot use variable in different context than where it was assigned");
}

Const::Const(types::Type *type) :
    Var(), type(type), outsMap(new std::map<SeqData, Value *>)
{
}

types::Type *Const::getType(Stage *caller) const
{
	return type;
}

ValMap Const::outs(Stage *caller) const
{
	return outsMap;
}


Stage *Const::getStage() const
{
	return nullptr;
}

bool Const::isAssigned() const
{
	return true;
}

BaseFunc *Const::getBase() const
{
	return nullptr;
}

ConstInt::ConstInt(seq_int_t n) : Const(types::IntType::get()), n(n)
{
}

ValMap ConstInt::outs(Stage *caller) const
{
	if (caller && outsMap->empty()) {
		LLVMContext& context = caller->getBase()->getContext();
		BasicBlock *preambleBlock = caller->getBase()->getPreamble();
		Value *var = makeAlloca(ConstantInt::get(seqIntLLVM(context), (uint64_t)n, true), preambleBlock);
		outsMap->insert({SeqData::INT, var});
	}

	return Const::outs(caller);
}

ConstInt& ConstInt::get(seq_int_t n)
{
	return *new ConstInt(n);
}

ConstFloat::ConstFloat(double f) : Const(types::FloatType::get()), f(f)
{
}

ValMap ConstFloat::outs(Stage *caller) const
{
	if (caller && outsMap->empty()) {
		LLVMContext& context = caller->getBase()->getContext();
		BasicBlock *preambleBlock = caller->getBase()->getPreamble();
		Value *var = makeAlloca(ConstantFP::get(Type::getDoubleTy(context), f), preambleBlock);
		outsMap->insert({SeqData::FLOAT, var});
	}

	return Const::outs(caller);
}

ConstFloat& ConstFloat::get(double f)
{
	return *new ConstFloat(f);
}

Latest::Latest() : Var()
{
}

static void validateCaller(Stage *caller)
{
	if (!caller)
		throw exc::SeqException("unexpected null stage");

	if (!caller->getPrev())
		throw exc::StageException("stage has no predecessor", *caller);
}

types::Type *Latest::getType(Stage *caller) const
{
	validateCaller(caller);
	return caller->getPrev()->getOutType();
}

ValMap Latest::outs(Stage *caller) const
{
	validateCaller(caller);
	return caller->getPrev()->outs;
}

Stage *Latest::getStage() const
{
	return nullptr;
}

bool Latest::isAssigned() const
{
	return true;
}

BaseFunc *Latest::getBase() const
{
	return nullptr;
}

Latest& Latest::get()
{
	static auto *latest = new Latest();
	return *latest;
}
