#include "seq.h"
#include "stage.h"
#include "basestage.h"
#include "exc.h"
#include "var.h"

using namespace seq;
using namespace llvm;

Var::Var() : assigned(false), pipeline(nullptr)
{
}

types::Type *Var::getType() const
{
	return pipeline->getTail()->getOutType();
}

std::shared_ptr<std::map<SeqData, Value *>> Var::outs() const
{
	return pipeline->getTail()->outs;
}

Pipeline& Var::operator|(Pipeline& to)
{
	if (!assigned)
		throw exc::SeqException("variable used before assigned");

	if (to.isAdded())
		throw exc::SeqException("cannot use same pipeline twice");

	to.getHead()->setBase(base);
	BaseStage& begin = BaseStage::make(types::Void::get(), getType());
	begin.setBase(base);
	begin.outs = pipeline->getTail()->outs;

	Pipeline& full = begin | to;
	base->add(&full);

	return full;
}

Pipeline& Var::operator|(Stage& to)
{
	return (*this | to.asPipeline());
}

Var& Var::operator=(Pipeline& to)
{
	if (assigned)
		throw exc::SeqException("variable cannot be assigned twice");

	assigned = true;
	base = to.getHead()->getBase();
	pipeline = &to;

	if (!pipeline->isAdded()) {
		BaseStage& begin = BaseStage::make(types::Void::get(), types::Void::get());
		begin.setBase(base);
		Pipeline& full = begin | to;
		base->add(&full);
	}

	return *this;
}

Var& Var::operator=(Stage& to)
{
	return *this = to.asPipeline();
}
