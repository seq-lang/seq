#include "seq/func.h"
#include "seq/return.h"

using namespace seq;
using namespace llvm;

Return::Return(Expr *expr) :
    Stage("return", types::AnyType::get(), types::VoidType::get()), expr(expr)
{
}

void Return::codegen(Module *module)
{
	ensurePrev();
	validate();

	block = prev->getAfter();
	getBase()->codegenReturn(expr, block);
	prev->setAfter(getAfter());
}

Return& Return::make(Expr *expr)
{
	return *new Return(expr);
}
