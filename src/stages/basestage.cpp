#include "basestage.h"

using namespace seq;
using namespace llvm;

BaseStage::BaseStage(types::Type *in, types::Type *out, Stage *proxy) :
    Stage("base", in, out), proxy(proxy)
{
}

BaseStage::BaseStage(types::Type *in, types::Type *out) :
    BaseStage(in, out, nullptr)
{
}

void BaseStage::codegen(Module *module, LLVMContext& context)
{
	validate();
	codegenNext(module, context);
}

types::Type *BaseStage::getOutType() const
{
	if (proxy && Stage::getOutType()->isChildOf(types::VoidType::get()))
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
