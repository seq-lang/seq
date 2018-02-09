#include "basestage.h"

using namespace seq;
using namespace llvm;

BaseStage::BaseStage(types::Type *in, types::Type *out) :
    Stage("Base", in, out)
{
}

void BaseStage::codegen(Module *module, LLVMContext& context)
{
	validate();
	codegenNext(module, context);
}

BaseStage& BaseStage::make(types::Type *in, types::Type *out)
{
	return *new BaseStage(in, out);
}
