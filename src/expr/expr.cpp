#include "seq/expr.h"

using namespace seq;
using namespace llvm;

Expr::Expr(types::Type *type) : type(type)
{
}

types::Type *Expr::getType() const
{
	return type;
}

void Expr::ensure(types::Type *type)
{
	if (!getType()->isChildOf(type))
		throw exc::SeqException("expected '" + type->getName() + "', got '" + getType()->getName() + "'");
}
