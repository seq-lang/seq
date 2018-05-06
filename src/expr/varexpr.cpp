#include "seq/varexpr.h"

using namespace seq;
using namespace llvm;

VarExpr::VarExpr(Var *var) : Expr(var->getType(nullptr)), var(var)
{
}

Value *VarExpr::codegen(BaseFunc *base, BasicBlock *block)
{
	return getType()->pack(base, var->outs(nullptr), block);
}

types::Type *VarExpr::getType() const
{
	return var->getType(nullptr);
}
