#include "seq/seq.h"
#include "seq/getelemexpr.h"

using namespace seq;
using namespace llvm;

GetElemExpr::GetElemExpr(Expr *rec, std::string memb) :
    rec(rec), memb(std::move(memb))
{
}

GetElemExpr::GetElemExpr(Expr *rec, seq_int_t idx) :
    GetElemExpr(rec, std::to_string(idx))
{
	assert(idx >= 1);
}

llvm::Value *GetElemExpr::codegen(BaseFunc *base, BasicBlock*& block)
{
	Value *rec = this->rec->codegen(base, block);
	return this->rec->getType()->memb(rec, memb, block);
}

types::Type *GetElemExpr::getType() const
{
	return rec->getType()->membType(memb);
}

GetElemExpr *GetElemExpr::clone(types::RefType *ref)
{
	return new GetElemExpr(rec->clone(ref), memb);
}
