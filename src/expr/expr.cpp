#include "seq/seq.h"
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
	types::Type *actual = getType();
	if (!actual->is(type) && !type->is(actual))
		throw exc::SeqException("expected '" + type->getName() + "', got '" + getType()->getName() + "'");
}

Expr *Expr::clone(Generic *ref)
{
	return this;
}

UOpExpr::UOpExpr(Op op, Expr *lhs) :
    Expr(), op(std::move(op)), lhs(lhs)
{
}

Value *UOpExpr::codegen(BaseFunc *base, BasicBlock*& block)
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

UOpExpr *UOpExpr::clone(Generic *ref)
{
	return new UOpExpr(op, lhs->clone(ref));
}

BOpExpr::BOpExpr(Op op, Expr *lhs, Expr *rhs) :
    Expr(), op(std::move(op)), lhs(lhs), rhs(rhs)
{
}

Value *BOpExpr::codegen(BaseFunc *base, BasicBlock*& block)
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

BOpExpr *BOpExpr::clone(Generic *ref)
{
	return new BOpExpr(op, lhs->clone(ref), rhs->clone(ref));
}

CondExpr::CondExpr(Expr *cond, Expr *ifTrue, Expr *ifFalse) :
    Expr(), cond(cond), ifTrue(ifTrue), ifFalse(ifFalse)
{
}

Value *CondExpr::codegen(BaseFunc *base, BasicBlock*& block)
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

CondExpr *CondExpr::clone(Generic *ref)
{
	return new CondExpr(cond->clone(ref), ifTrue->clone(ref), ifFalse->clone(ref));
}

MatchExpr::MatchExpr() :
    Expr(), value(nullptr), patterns(), exprs()
{
}

void MatchExpr::setValue(Expr *value)
{
	assert(!this->value);
	this->value = value;
}

void MatchExpr::addCase(Pattern *pattern, Expr *expr)
{
	patterns.push_back(pattern);
	exprs.push_back(expr);
}

Value *MatchExpr::codegen(BaseFunc *base, BasicBlock *&block)
{
	assert(!patterns.empty());
	assert(patterns.size() == exprs.size() && value);

	LLVMContext& context = block->getContext();
	Function *func = block->getParent();

	IRBuilder<> builder(block);
	types::Type *valType = value->getType();
	types::Type *resType = getType();

	bool seenCatchAll = false;
	for (auto *pattern : patterns) {
		pattern->validate(value->getType());
		if (pattern->isCatchAll())
			seenCatchAll = true;
	}

	if (!seenCatchAll)
		throw exc::SeqException("match expression missing catch-all pattern");

	Value *val = value->codegen(base, block);

	std::vector<std::pair<BranchInst *, Value *>> binsts;

	for (unsigned i = 0; i < patterns.size(); i++) {
		Value *cond = patterns[i]->codegen(base, valType, val, block);

		builder.SetInsertPoint(block);  // recall: expr codegen can change the block
		block = BasicBlock::Create(context, "", func);  // match block
		BranchInst *binst1 = builder.CreateCondBr(cond, block, block);  // we set false-branch below

		Value *result = exprs[i]->codegen(base, block);
		builder.SetInsertPoint(block);
		BranchInst *binst2 = builder.CreateBr(block);  // we reset this below
		binsts.emplace_back(binst2, result);

		block = BasicBlock::Create(context, "", func);  // mismatch block (eval next pattern)
		binst1->setSuccessor(1, block);
	}

	builder.SetInsertPoint(block);
	builder.CreateUnreachable();

	block = BasicBlock::Create(context, "", func);
	builder.SetInsertPoint(block);

	PHINode *result = builder.CreatePHI(resType->getLLVMType(context), (unsigned)patterns.size());
	for (auto& binst : binsts) {
		binst.first->setSuccessor(0, block);
		result->addIncoming(binst.second, binst.first->getParent());
	}

	return result;
}

types::Type *MatchExpr::getType() const
{
	assert(!exprs.empty());
	types::Type *type = exprs[0]->getType();

	for (auto *expr : exprs) {
		if (!expr->getType()->is(type) && !type->is(expr->getType()))
			throw exc::SeqException("inconsistent result types in match expression");
	}

	return type;
}

MatchExpr *MatchExpr::clone(Generic *ref)
{
	auto *x = new MatchExpr();

	std::vector<Pattern *> patternsCloned;
	std::vector<Expr *> exprsCloned;

	for (auto *pattern : patterns)
		patternsCloned.push_back(pattern->clone(ref));

	for (auto *expr : exprs)
		exprsCloned.push_back(expr->clone(ref));

	if (value) x->value = value->clone(ref);
	x->patterns = patternsCloned;
	x->exprs = exprsCloned;

	return x;
}

ConstructExpr::ConstructExpr(types::Type *type, std::vector<Expr *> args) :
    Expr(), type(type), args(std::move(args))
{
}

Value *ConstructExpr::codegen(BaseFunc *base, BasicBlock*& block)
{
	std::vector<Value *> vals;
	for (auto *arg : args)
		vals.push_back(arg->codegen(base, block));
	return type->construct(base, vals, block);
}

types::Type *ConstructExpr::getType() const
{
	std::vector<types::Type *> types;
	for (auto *arg : args)
		types.push_back(arg->getType());
	return type->getConstructType(types);
}

ConstructExpr *ConstructExpr::clone(Generic *ref)
{
	std::vector<Expr *> argsCloned;
	for (auto *arg : args)
		argsCloned.push_back(arg->clone(ref));
	return new ConstructExpr(type->clone(ref), argsCloned);
}

OptExpr::OptExpr(Expr *val) : Expr(), val(val)
{
}

Value *OptExpr::codegen(BaseFunc *base, BasicBlock*& block)
{
	Value *val = this->val->codegen(base, block);
	return ((types::OptionalType *)getType())->make(val, block);
}

types::Type *OptExpr::getType() const
{
	return types::OptionalType::get(val->getType());
}

OptExpr *OptExpr::clone(Generic *ref)
{
	return new OptExpr(val->clone(ref));
}

DefaultExpr::DefaultExpr(types::Type *type) : Expr(type)
{
}

Value *DefaultExpr::codegen(BaseFunc *base, BasicBlock*& block)
{
	return getType()->defaultValue(block);
}

DefaultExpr *DefaultExpr::clone(Generic *ref)
{
	return new DefaultExpr(getType()->clone(ref));
}
