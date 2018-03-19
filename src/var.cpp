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

std::shared_ptr<std::map<SeqData, Value *>> Var::outs(Stage *caller) const
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

Seq *Var::getBase() const
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

	Seq *base = getBase();
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
	Seq *base = getBase();

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
	return LoadStore::make(this, &idx);
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

std::shared_ptr<std::map<SeqData, Value *>> Latest::outs(Stage *caller) const
{
	validateCaller(caller);
	return caller->getPrev()->outs;
};

Stage *Latest::getStage() const
{
	throw exc::SeqException("cannot get stage of _ variable");
}

bool Latest::isAssigned() const
{
	return true;
}

Seq *Latest::getBase() const
{
	throw exc::SeqException("cannot get base of _ variable");
}

Latest& Latest::get()
{
	static auto *latest = new Latest();
	return *latest;
}
