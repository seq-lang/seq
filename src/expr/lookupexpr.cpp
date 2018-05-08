#include "seq/seq.h"
#include "seq/lookupexpr.h"

using namespace seq;
using namespace llvm;

ArrayLookupExpr::ArrayLookupExpr(Expr *arr, Expr *idx) :
    arr(arr), idx(idx)
{
	idx->ensure(types::IntType::get());
}

Value *ArrayLookupExpr::codegen(BaseFunc *base, BasicBlock *block)
{
	IRBuilder<> builder(block);

	auto outs = std::make_shared<std::map<SeqData, Value *>>(*new std::map<SeqData, Value *>());
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
