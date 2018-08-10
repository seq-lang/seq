#include "seq/seq.h"
#include "seq/arrayexpr.h"

using namespace seq;
using namespace llvm;

ArrayExpr::ArrayExpr(types::Type *type, Expr *count) :
    Expr(types::ArrayType::get(type)), count(count)
{
}

Value *ArrayExpr::codegen(BaseFunc *base, BasicBlock*& block)
{
	auto *type = dynamic_cast<types::ArrayType *>(getType());
	assert(type != nullptr);
	count->ensure(types::IntType::get());

	Value *len = count->codegen(base, block);
	Value *ptr = type->indexType()->alloc(len, block);
	Value *arr = type->make(ptr, len, block);
	return arr;
}

ArrayExpr *ArrayExpr::clone(Generic *ref)
{
	return new ArrayExpr(getType()->clone(ref)->getBaseType(0), count->clone(ref));
}
