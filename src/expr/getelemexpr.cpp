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

GetElemExpr *GetElemExpr::clone(Generic *ref)
{
	return new GetElemExpr(rec->clone(ref), memb);
}

GetStaticElemExpr::GetStaticElemExpr(types::Type *type, std::string memb) :
    Expr(), type(type), memb(std::move(memb))
{
}

Value *GetStaticElemExpr::codegen(BaseFunc *base, BasicBlock*& block)
{
	return type->staticMemb(memb, block);
}

types::Type *GetStaticElemExpr::getType() const
{
	return type->staticMembType(memb);
}

GetStaticElemExpr *GetStaticElemExpr::clone(Generic *ref)
{
	return new GetStaticElemExpr(type->clone(ref), memb);
}
