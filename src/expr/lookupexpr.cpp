#include "seq/seq.h"
#include "seq/lookupexpr.h"

using namespace seq;
using namespace llvm;

static types::ArrayType *asArrayType(types::Type *type)
{
	types::ArrayType *arrType;
	if ((arrType = dynamic_cast<types::ArrayType *>(type)))
		return arrType;
	throw exc::SeqException("expected array, got '" + type->getName() + "'");
}

ArrayLookupExpr::ArrayLookupExpr(Expr *arr, Expr *idx) :
    Expr(asArrayType(arr->getType())->getBaseType()), arr(arr), idx(idx)
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

	asArrayType(this->arr->getType())->codegenIndexLoad(base, outs, block, ptr, idx);
	return getType()->pack(base, outs, block);
}
