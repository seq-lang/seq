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
	auto ins = makeValMap();
	auto outs = makeValMap();
	Value *f = func->codegen(base, block);
	Value *x = arg ? arg->codegen(base, block) : nullptr;
	if (arg)
		arg->getType()->unpack(base, x, ins, block);
	func->getType()->call(base, ins, outs, f, block);
	return getType()->pack(base, outs, block);
}

types::Type *CallExpr::getType() const
{
	return func->getType()->getCallType();
}
