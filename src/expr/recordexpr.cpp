#include <seq/record.h>
#include <seq/void.h>
#include "seq/recordexpr.h"

using namespace seq;
using namespace llvm;

RecordExpr::RecordExpr(std::vector<Expr *> exprs) : Expr(types::VoidType::get()), exprs(std::move(exprs))
{
}

Value *RecordExpr::codegen(BaseFunc *base, BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	types::Type *type = getType();
	Value *rec = UndefValue::get(type->getLLVMType(context));
	unsigned idx = 0;

	IRBuilder<> builder(block);
	for (auto *expr : exprs) {
		Value *val = expr->codegen(base, block);
		rec = builder.CreateInsertValue(rec, val, idx++);
	}

	return rec;
}

types::Type *RecordExpr::getType() const
{
	std::vector<types::Type *> types;
	for (auto *expr : exprs)
		types.push_back(expr->getType());
	return types::RecordType::get(types);
}
