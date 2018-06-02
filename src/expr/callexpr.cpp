#include "seq/seq.h"
#include "seq/callexpr.h"

using namespace seq;
using namespace llvm;

CallExpr::CallExpr(Expr *func, Expr *arg) :
    func(func), arg(arg)
{
}

Value *CallExpr::codegen(BaseFunc *base, BasicBlock*& block)
{
	Value *f = func->codegen(base, block);
	Value *x = arg ? arg->codegen(base, block) : nullptr;
	return func->getType()->call(base, f, x, block);
}

types::Type *CallExpr::getType() const
{
	return func->getType()->getCallType(arg ? arg->getType() : types::VoidType::get());
}
