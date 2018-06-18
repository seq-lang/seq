#include "seq/func.h"
#include "seq/basestage.h"

using namespace seq;
using namespace llvm;

static int idx = 1;

BaseStage::BaseStage(types::Type *in,
                     types::Type *out,
                     Stage *proxy,
                     bool init) :
    Stage("base" + std::to_string(idx++), in, out),
    proxy(proxy), deferredResult(nullptr)
{
	this->init = init;
}

BaseStage::BaseStage(types::Type *in, types::Type *out, bool init) :
    BaseStage(in, out, nullptr)
{
	this->init = init;
}

void BaseStage::codegen(Module *module)
{
	validate();

	if (prev && !block)
		block = prev->getAfter();

	if (!result && deferredResult)
		result = *deferredResult;

	if (init) codegenInit(block);
	codegenNext(module);
	if (init) finalizeInit();

	if (prev)
		prev->setAfter(getAfter());
}

types::Type *BaseStage::getOutType() const
{
	if (proxy && Stage::getOutType()->is(types::VoidType::get()))
		return proxy->getOutType();
	else
		return Stage::getOutType();
}

void BaseStage::deferResult(Value **result)
{
	deferredResult = result;
}

BaseStage& BaseStage::make(types::Type *in,
                           types::Type *out,
                           Stage *proxy,
                           bool init)
{
	return *new BaseStage(in, out, proxy, init);
}

BaseStage& BaseStage::make(types::Type *in, types::Type *out, bool init)
{
	return *new BaseStage(in, out, init);
}

BaseStage *BaseStage::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (BaseStage *)ref->getClone(this);

	BaseStage& x = proxy ? BaseStage::make(in->clone(ref), out->clone(ref), proxy->clone(ref), init) :
	                       BaseStage::make(in->clone(ref), out->clone(ref), init);
	ref->addClone(this, &x);
	Stage::setCloneBase(&x, ref);
	return &x;
}
