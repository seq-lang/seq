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
	types::Type *type = arr->getType();
	types::ArrayType *arrType;

	if ((arrType = dynamic_cast<types::ArrayType *>(type)))
		return arrType->getBaseType();

	throw exc::SeqException("expected array, got '" + type->getName() + "'");
}

ArrayLookupExpr *ArrayLookupExpr::clone(types::RefType *ref)
{
	return new ArrayLookupExpr(arr->clone(ref), idx->clone(ref));
}
