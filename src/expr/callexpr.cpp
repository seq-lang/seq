#include "seq/seq.h"
#include "seq/callexpr.h"

using namespace seq;
using namespace llvm;

CallExpr::CallExpr(Expr *func, std::vector<Expr *> args) :
    func(func), args(std::move(args))
{
}

Value *CallExpr::codegen(BaseFunc *base, BasicBlock*& block)
{
	Value *f = func->codegen(base, block);
	std::vector<Value *> x;
	for (auto *e : args)
		x.push_back(e->codegen(base, block));
	return func->getType()->call(base, f, x, block);
}

types::Type *CallExpr::getType() const
{
	std::vector<types::Type *> types;
	for (auto *e : args)
		types.push_back(e->getType());
	return func->getType()->getCallType(types);
}

CallExpr *CallExpr::clone(types::RefType *ref)
{
	std::vector<Expr *> argsCloned;
	for (auto *arg : args)
		argsCloned.push_back(arg->clone(ref));
	return new CallExpr(func->clone(ref), argsCloned);
}
