#include "seq/varexpr.h"

using namespace seq;
using namespace llvm;

VarExpr::VarExpr(Var *var) : var(var)
{
}

Value *VarExpr::codegen(BaseFunc *base, BasicBlock*& block)
{
	return var->load(base, block);
}

types::Type *VarExpr::getType() const
{
	return var->getType();
}

VarExpr *VarExpr::clone(Generic *ref)
{
	return new VarExpr(var->clone(ref));
}

FuncExpr::FuncExpr(BaseFunc *func, std::vector<types::Type *> types) :
    func(func), types(std::move(types))
{
}

bool FuncExpr::isParameterized()
{
	return !types.empty();
}

BaseFunc *FuncExpr::getFunc()
{
	return func;
}

Value *FuncExpr::codegen(BaseFunc *base, BasicBlock*& block)
{
	auto *f = dynamic_cast<Func *>(func);
	if (f) {
		if (!types.empty())
			func = f->realize(types);
	} else if (!types.empty()) {
		throw exc::SeqException("cannot type-instantiate non-generic function");
	}

	func->codegen(block->getModule());
	return func->getFunc();
}

types::Type *FuncExpr::getType() const
{
	return func->getFuncType();
}

FuncExpr *FuncExpr::clone(Generic *ref)
{
	return new FuncExpr(func->clone(ref));
}
