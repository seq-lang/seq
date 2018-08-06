#include "seq/varexpr.h"

using namespace seq;
using namespace llvm;

CellExpr::CellExpr(Cell *cell) : cell(cell)
{
}

Value *CellExpr::codegen(BaseFunc *base, BasicBlock*& block)
{
	return cell->load(base, block);
}

types::Type *CellExpr::getType() const
{
	return cell->getType();
}

CellExpr *CellExpr::clone(types::RefType *ref)
{
	return new CellExpr(cell->clone(ref));
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
