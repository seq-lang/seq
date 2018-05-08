#include "seq/seq.h"
#include "seq/getelemexpr.h"

using namespace seq;
using namespace llvm;

GetElemExpr::GetElemExpr(Expr *rec, seq_int_t idx) :
    rec(rec), idx(idx)
{
	assert(idx >= 1);
}

llvm::Value *GetElemExpr::codegen(BaseFunc *base, BasicBlock *block)
{
	rec->ensure(types::RecordType::get({}));
	IRBuilder<> builder(block);
	Value *rec = this->rec->codegen(base, block);
	return builder.CreateExtractValue(rec, idx - 1);
}

types::Type *GetElemExpr::getType() const
{
	return rec->getType()->getBaseType(idx);
}
