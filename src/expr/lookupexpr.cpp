#include "seq/seq.h"
#include "seq/lookupexpr.h"

using namespace seq;
using namespace llvm;

ArrayLookupExpr::ArrayLookupExpr(Expr *arr, Expr *idx) :
    arr(arr), idx(idx)
{
}

Value *ArrayLookupExpr::codegen(BaseFunc *base, BasicBlock*& block)
{
	idx->ensure(types::IntType::get());
	Value *arr = this->arr->codegen(base, block);
	Value *idx = this->idx->codegen(base, block);
	return this->arr->getType()->indexLoad(base, arr, idx, block);
}

types::Type *ArrayLookupExpr::getType() const
{
	return arr->getType()->indexType();
}

ArrayLookupExpr *ArrayLookupExpr::clone(types::RefType *ref)
{
	return new ArrayLookupExpr(arr->clone(ref), idx->clone(ref));
}

ArraySliceExpr::ArraySliceExpr(Expr *arr, Expr *from, Expr *to) :
    arr(arr), from(from), to(to)
{
}

Value *ArraySliceExpr::codegen(BaseFunc *base, BasicBlock*& block)
{
	assert(from || to);
	if (from) from->ensure(types::IntType::get());
	if (to) to->ensure(types::IntType::get());

	Value *arr = this->arr->codegen(base, block);

	if (!from) {
		Value *to = this->to->codegen(base, block);
		return this->arr->getType()->indexSliceNoFrom(base, arr, to, block);
	} else if (!to) {
		Value *from = this->from->codegen(base, block);
		return this->arr->getType()->indexSliceNoTo(base, arr, from, block);
	} else {
		Value *from = this->from->codegen(base, block);
		Value *to = this->to->codegen(base, block);
		return this->arr->getType()->indexSlice(base, arr, from, to, block);
	}
}

types::Type *ArraySliceExpr::getType() const
{
	return arr->getType();
}

ArraySliceExpr *ArraySliceExpr::clone(types::RefType *ref)
{
	return new ArraySliceExpr(arr->clone(ref), from->clone(ref), to->clone(ref));
}
