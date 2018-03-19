#include "seq.h"
#include "stage.h"
#include "basestage.h"
#include "mem.h"
#include "exc.h"
#include "var.h"

using namespace seq;
using namespace llvm;

Var::Var() : assigned(false), stage(nullptr)
{
}

Var::Var(Pipeline pipeline) : Var()
{
	*this = pipeline;
}

types::Type *Var::getType(Stage *caller) const
{
	return stage->getOutType();
}

std::shared_ptr<std::map<SeqData, Value *>> Var::outs(Stage *caller) const
{
	return stage->outs;
}

Seq *Var::getBase() const
{
	return base;
}

Pipeline Var::operator|(Pipeline to)
{
	if (!assigned)
		throw exc::SeqException("variable used before assigned");

	if (to.isAdded())
		throw exc::SeqException("cannot use same pipeline twice");

	to.getHead()->setBase(base);
	BaseStage& begin = BaseStage::make(types::VoidType::get(), getType(stage));
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
	if (assigned)
		throw exc::SeqException("variable cannot be assigned twice");

	assigned = true;
	base = to.getHead()->getBase();
	stage = to.getTail();

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

Latest& Latest::get()
{
	static auto *latest = new Latest();
	return *latest;
}
