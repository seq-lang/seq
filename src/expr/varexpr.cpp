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

VarExpr *VarExpr::clone(types::RefType *ref)
{
	return new VarExpr(var->clone(ref));
}

FuncExpr::FuncExpr(BaseFunc *func) : func(func)
{
}

Value *FuncExpr::codegen(BaseFunc *base, BasicBlock*& block)
{
	func->codegen(block->getModule());
	return func->getFunc();
}

types::Type *FuncExpr::getType() const
{
	return func->getFuncType();
}

FuncExpr *FuncExpr::clone(types::RefType *ref)
{
	return new FuncExpr(func->clone(ref));
}
