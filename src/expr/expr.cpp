#include <seq/void.h>
#include "seq/expr.h"

using namespace seq;
using namespace llvm;

Expr::Expr(types::Type *type) : type(type)
{
}

Expr::Expr() : Expr(types::VoidType::get())
{
}

types::Type *Expr::getType() const
{
	return type;
}

void Expr::ensure(types::Type *type)
{
	if (!getType()->isGeneric(type))
		throw exc::SeqException("expected '" + type->getName() + "', got '" + getType()->getName() + "'");
}

UOpExpr::UOpExpr(Op op, Expr *lhs) :
    Expr(), op(std::move(op)), lhs(lhs)
{
}

llvm::Value *UOpExpr::codegen(BaseFunc *base, BasicBlock *block)
{
	auto spec = lhs->getType()->findUOp(op.symbol);
	IRBuilder<> builder(block);
	Value *lhs = this->lhs->codegen(base, block);
	return spec.codegen(lhs, nullptr, builder);
}

types::Type *UOpExpr::getType() const
{
	return lhs->getType()->findUOp(op.symbol).outType;
}

BOpExpr::BOpExpr(Op op, Expr *lhs, Expr *rhs) :
    Expr(), op(std::move(op)), lhs(lhs), rhs(rhs)
{
}

llvm::Value *BOpExpr::codegen(BaseFunc *base, BasicBlock *block)
{
	auto spec = lhs->getType()->findBOp(op.symbol, rhs->getType());
	IRBuilder<> builder(block);
	Value *lhs = this->lhs->codegen(base, block);
	Value *rhs = this->rhs->codegen(base, block);
	return spec.codegen(lhs, rhs, builder);
}

types::Type *BOpExpr::getType() const
{
	return lhs->getType()->findBOp(op.symbol, rhs->getType()).outType;
}
