#include "seq.h"
#include "stage.h"
#include "basestage.h"
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

types::Type *Var::getType() const
{
	return stage->getOutType();
}

std::shared_ptr<std::map<SeqData, Value *>> Var::outs() const
{
	return stage->outs;
}

Pipeline Var::operator|(Pipeline to)
{
	if (!assigned)
		throw exc::SeqException("variable used before assigned");

	if (to.isAdded())
		throw exc::SeqException("cannot use same pipeline twice");

	to.getHead()->setBase(base);
	BaseStage& begin = BaseStage::make(types::Void::get(), getType());
	begin.setBase(base);
	begin.outs = stage->outs;

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
		BaseStage& begin = BaseStage::make(types::Void::get(), types::Void::get());
		begin.setBase(base);
		Pipeline full = begin | to;
		base->add(full);
	}

	return *this;
}
