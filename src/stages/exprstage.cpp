#include <seq/record.h>
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
	Stage::validate();
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

AssignIndexStage::AssignIndexStage(Expr *array, Expr *idx, Expr *value) :
    Stage("([]=)", types::AnyType::get(), types::VoidType::get()),
    array(array), idx(idx), value(value)
{
}

void AssignIndexStage::codegen(Module *module)
{
	this->idx->ensure(types::IntType::get());
	this->idx->ensure(types::IntType::get());

	if (!array->getType()->isGeneric(types::ArrayType::get()))
		throw exc::SeqException("can only assign indices of array type");

	auto *arrType = dynamic_cast<types::ArrayType *>(array->getType());
	assert(arrType != nullptr);
	value->ensure(arrType->getBaseType());

	ensurePrev();
	validate();

	block = prev->getAfter();
	IRBuilder<> builder(block);

	auto outs = makeValMap();
	Value *val = value->codegen(getBase(), block);
	Value *arr = array->codegen(getBase(), block);
	Value *ptr = builder.CreateExtractValue(arr, 1);
	Value *idx = this->idx->codegen(getBase(), block);
	value->getType()->unpack(getBase(), val, outs, block);
	array->getType()->codegenIndexStore(getBase(), outs, block, ptr, idx);

	codegenNext(module);
	prev->setAfter(getAfter());
}

AssignIndexStage& AssignIndexStage::make(Expr *array, Expr *idx, Expr *value)
{
	return *new AssignIndexStage(array, idx, value);
}


AssignMemberStage::AssignMemberStage(Cell *cell, seq_int_t idx, Expr *value) :
    Stage("(.=)", types::AnyType::get(), types::VoidType::get()),
    cell(cell), idx(idx), value(value)
{
}

void AssignMemberStage::codegen(Module *module)
{
	if (!cell->getType()->isGeneric(types::RecordType::get({})))
		throw exc::SeqException("can only assign members of record type");

	value->ensure(cell->getType()->getBaseType(idx));

	ensurePrev();
	validate();

	block = prev->getAfter();
	IRBuilder<> builder(block);
	Value *rec = cell->load(block);
	rec = builder.CreateInsertValue(rec, value->codegen(getBase(), block), idx - 1);
	cell->store(rec, block);
	codegenNext(module);
	prev->setAfter(getAfter());
}

AssignMemberStage& AssignMemberStage::make(Cell *cell, seq_int_t idx, Expr *value)
{
	return *new AssignMemberStage(cell, idx, value);
}
