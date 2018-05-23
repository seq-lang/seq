#include "seq/void.h"
#include "seq/num.h"
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

llvm::Value *UOpExpr::codegen(BaseFunc *base, BasicBlock*& block)
{
	auto spec = lhs->getType()->findUOp(op.symbol);
	Value *lhs = this->lhs->codegen(base, block);
	IRBuilder<> builder(block);
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

llvm::Value *BOpExpr::codegen(BaseFunc *base, BasicBlock*& block)
{
	LLVMContext& context = block->getContext();

	/*
	 * && and || are special-cased because of short-circuiting
	 */
	if (op == bop("&&") || op == bop("||")) {
		const bool isAnd = (op == bop("&&"));

		lhs->ensure(types::BoolType::get());
		rhs->ensure(types::BoolType::get());
		Value *lhs = this->lhs->codegen(base, block);

		BasicBlock *b1 = BasicBlock::Create(context, "", block->getParent());

		IRBuilder<> builder(block);
		lhs = builder.CreateTrunc(lhs, IntegerType::getInt1Ty(context));
		BranchInst *branch = builder.CreateCondBr(lhs, b1, b1);  // one branch changed below

		Value *rhs = this->rhs->codegen(base, b1);
		builder.SetInsertPoint(b1);

		BasicBlock *b2 = BasicBlock::Create(context, "", block->getParent());
		builder.CreateBr(b2);
		builder.SetInsertPoint(b2);

		Type *boolTy = types::BoolType::get()->getLLVMType(context);
		Value *t = ConstantInt::get(boolTy, 1);
		Value *f = ConstantInt::get(boolTy, 0);

		PHINode *result = builder.CreatePHI(boolTy, 2);
		result->addIncoming(isAnd ? f : t, block);
		result->addIncoming(rhs, b1);

		branch->setSuccessor(isAnd ? 1 : 0, b2);
		block = b2;
		return result;
	} else {
		auto spec = lhs->getType()->findBOp(op.symbol, rhs->getType());
		Value *lhs = this->lhs->codegen(base, block);
		Value *rhs = this->rhs->codegen(base, block);
		IRBuilder<> builder(block);
		return spec.codegen(lhs, rhs, builder);
	}
}

types::Type *BOpExpr::getType() const
{
	if (op == bop("&&") || op == bop("||"))
		return types::BoolType::get();
	else
		return lhs->getType()->findBOp(op.symbol, rhs->getType()).outType;
}

CondExpr::CondExpr(Expr *cond, Expr *ifTrue, Expr *ifFalse) :
    Expr(), cond(cond), ifTrue(ifTrue), ifFalse(ifFalse)
{
}

llvm::Value *CondExpr::codegen(BaseFunc *base, BasicBlock*& block)
{
	cond->ensure(types::BoolType::get());

	LLVMContext& context = block->getContext();

	Value *cond = this->cond->codegen(base, block);
	IRBuilder<> builder(block);
	cond = builder.CreateTrunc(cond, IntegerType::getInt1Ty(context));

	BasicBlock *b1 = BasicBlock::Create(context, "", block->getParent());
	BranchInst *branch0 = builder.CreateCondBr(cond, b1, b1);  // we set false-branch below

	Value *ifTrue = this->ifTrue->codegen(base, b1);
	builder.SetInsertPoint(b1);
	BranchInst *branch1 = builder.CreateBr(b1);  // changed below

	BasicBlock *b2 = BasicBlock::Create(context, "", block->getParent());
	branch0->setSuccessor(1, b2);
	Value *ifFalse = this->ifFalse->codegen(base, b2);
	builder.SetInsertPoint(b2);
	BranchInst *branch2 = builder.CreateBr(b2);  // changed below

	block = BasicBlock::Create(context, "", block->getParent());
	branch1->setSuccessor(0, block);
	branch2->setSuccessor(0, block);
	builder.SetInsertPoint(block);
	PHINode *result = builder.CreatePHI(getType()->getLLVMType(context), 2);
	result->addIncoming(ifTrue, b1);
	result->addIncoming(ifFalse, b2);
	return result;
}

types::Type *CondExpr::getType() const
{
	if (!ifTrue->getType()->is(ifFalse->getType()))
		throw exc::SeqException("inconsistent types '" + ifTrue->getType()->getName() + "' and '" +
		                        ifFalse->getType()->getName() + "' in conditional expression");

	return ifTrue->getType();
}
