#include "seq/exprstage.h"

using namespace seq;
using namespace llvm;

ExprStage::ExprStage(Expr *expr) :
    Stage("expr", types::AnyType::get(), types::VoidType::get()), expr(expr)
{
}

void ExprStage::validate()
{
	out = expr->getType();
}

void ExprStage::codegen(Module *module)
{
	ensurePrev();
	validate();

	block = prev->getAfter();
	Value *val = expr->codegen(getBase(), block);
	getOutType()->unpack(getBase(), val, outs, block);
	codegenNext(module);
	prev->setAfter(getAfter());
}

ExprStage& ExprStage::make(Expr *expr)
{
	return *new ExprStage(expr);
}

CellStage::CellStage(Cell *cell) :
    Stage("cell", types::AnyType::get(), types::VoidType::get()), cell(cell)
{
}

void CellStage::codegen(Module *module)
{
	ensurePrev();
	validate();

	block = prev->getAfter();
	cell->codegen(block);
	codegenNext(module);
	prev->setAfter(getAfter());
}

CellStage& CellStage::make(Cell *cell)
{
	return *new CellStage(cell);
}

AssignStage::AssignStage(Cell *cell, Expr *value) :
    Stage("(=)", types::AnyType::get(), types::VoidType::get()), cell(cell), value(value)
{
}

void AssignStage::codegen(Module *module)
{
	value->ensure(cell->getType());

	ensurePrev();
	validate();

	block = prev->getAfter();
	cell->store(value->codegen(getBase(), block), block);
	codegenNext(module);
	prev->setAfter(getAfter());
}

AssignStage& AssignStage::make(Cell *cell, Expr *value)
{
	return *new AssignStage(cell, value);
}
