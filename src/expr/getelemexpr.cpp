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

MethodExpr::MethodExpr(Expr *expr, std::string name, std::vector<types::Type *> types) :
    Expr(), expr(expr), name(std::move(name)), types(std::move(types))
{
}

Value *MethodExpr::codegen(BaseFunc *base, llvm::BasicBlock*& block)
{
	types::Type *type = expr->getType();
	auto *func = dynamic_cast<Func *>(type->getMethod(name));

	if (!func)
		throw exc::SeqException("method '" + name + "' of type '" + type->getName() + "' is not generic");

	Value *self = expr->codegen(base, block);

	if (!types.empty())
		func = func->realize(types);

	Value *method = FuncExpr(func).codegen(base, block);
	return getType()->make(self, method, block);
}

types::MethodType *MethodExpr::getType() const
{
	types::Type *type = expr->getType();
	auto *func = dynamic_cast<Func *>(type->getMethod(name));

	if (!func)
		throw exc::SeqException("method '" + name + "' of type '" + type->getName() + "' is not generic");

	if (!types.empty())
		func = func->realize(types);

	return types::MethodType::get(expr->getType(), func->getFuncType());
}

MethodExpr *MethodExpr::clone(Generic *ref)
{
	std::vector<types::Type *> typesCloned;
	for (auto *type : types)
		typesCloned.push_back(type->clone(ref));

	return new MethodExpr(expr->clone(ref), name, typesCloned);
}
