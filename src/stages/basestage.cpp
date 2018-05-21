#include "seq/basestage.h"

using namespace seq;
using namespace llvm;

static int idx = 1;

BaseStage::BaseStage(types::Type *in, types::Type *out, Stage *proxy) :
    Stage("base" + std::to_string(idx++), in, out), proxy(proxy)
{
}

BaseStage::BaseStage(types::Type *in, types::Type *out) :
    BaseStage(in, out, nullptr)
{
}

void BaseStage::codegen(Module *module)
{
	validate();

	if (prev && !block)
		block = prev->getAfter();

	codegenNext(module);

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

BaseStage& BaseStage::make(types::Type *in, types::Type *out, Stage *proxy)
{
	return *new BaseStage(in, out, proxy);
}

BaseStage& BaseStage::make(types::Type *in, types::Type *out)
{
	return *new BaseStage(in, out);
}
