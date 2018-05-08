#include <seq/void.h>
#include "seq/expr.h"

using namespace seq;
using namespace llvm;

Expr::Expr(types::Type *type) : type(type)
{
}

Expr::Expr() : Expr(types::VoidType::get())
{
}

types::Type *Expr::getType() const
{
	return type;
}

void Expr::ensure(types::Type *type)
{
	if (!getType()->isGeneric(type))
		throw exc::SeqException("expected '" + type->getName() + "', got '" + getType()->getName() + "'");
}
