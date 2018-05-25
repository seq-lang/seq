#include "seq/varexpr.h"

using namespace seq;
using namespace llvm;

VarExpr::VarExpr(Var *var) : var(var)
{
}

Value *VarExpr::codegen(BaseFunc *base, BasicBlock*& block)
{
	return getType()->pack(base, var->outs(nullptr), block);
}

types::Type *VarExpr::getType() const
{
	return var->getType(nullptr);
}

CellExpr::CellExpr(Cell *cell) : cell(cell)
{
}

Value *CellExpr::codegen(BaseFunc *base, BasicBlock*& block)
{
	return cell->load(block);
}

types::Type *CellExpr::getType() const
{
	return cell->getType();
}

FuncExpr::FuncExpr(Func *func) : func(func)
{
}

Value *FuncExpr::codegen(BaseFunc *base, BasicBlock*& block)
{
	func->codegen(block->getModule());
	return func->getFunc();
}

types::Type *FuncExpr::getType() const
{
	return types::FuncType::get(func->getInType(), func->getOutType());
}
