#include "seq/seq.h"
#include "seq/lookupexpr.h"

using namespace seq;
using namespace llvm;

ArrayLookupExpr::ArrayLookupExpr(Expr *arr, Expr *idx) :
    arr(arr), idx(idx)
{
}

Value *ArrayLookupExpr::codegen(BaseFunc *base, BasicBlock *block)
{
	idx->ensure(types::IntType::get());
	IRBuilder<> builder(block);

	auto outs = makeValMap();
	Value *arr = this->arr->codegen(base, block);
	Value *idx = this->idx->codegen(base, block);
	Value *ptr = builder.CreateExtractValue(arr, 1);

	types::ArrayType::get(getType())->codegenIndexLoad(base, outs, block, ptr, idx);
	return getType()->pack(base, outs, block);
}

types::Type *ArrayLookupExpr::getType() const
{
	types::Type *type = arr->getType();
	types::ArrayType *arrType;

	if ((arrType = dynamic_cast<types::ArrayType *>(type)))
		return arrType->getBaseType();

	throw exc::SeqException("expected array, got '" + type->getName() + "'");
}

Expr *ArrayLookupExpr::getArr()
{
	return arr;
}

Expr *ArrayLookupExpr::getIdx()
{
	return idx;
}
