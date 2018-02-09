#include "stage.h"
#include "basestage.h"
#include "exc.h"
#include "var.h"

using namespace seq;
using namespace llvm;

Var::Var() : Var(types::Base::get())
{
}

Var::Var(types::Type *type) :
    assigned(false), type(type), pipeline(nullptr)
{
}

Pipeline& Var::operator|(Pipeline& to)
{
	if (!assigned)
		throw exc::SeqException("variable used before assigned");

	if (to.isAdded())
		throw exc::SeqException("cannot use same pipeline twice");

	to.getHead()->setBase(base);
	BaseStage& begin = BaseStage::make(types::Void::get(), type);
	begin.setBase(base);
	begin.outs = pipeline->getTail()->outs;

	Pipeline& add = begin | to;
	add.setAdded();
	base->add(&add);

	return add;
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
	type = to.getTail()->getOutType();
	base = to.getHead()->getBase();
	pipeline = &to;

	return *this;
}

Var& Var::operator=(Stage& to)
{
	return *this = to.asPipeline();
}
