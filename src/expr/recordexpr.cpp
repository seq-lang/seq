#include "seq/record.h"
#include "seq/void.h"
#include "seq/recordexpr.h"

using namespace seq;
using namespace llvm;

RecordExpr::RecordExpr(std::vector<Expr *> exprs, std::vector<std::string> names) :
    exprs(std::move(exprs)), names(std::move(names))
{
}

Value *RecordExpr::codegen(BaseFunc *base, BasicBlock*& block)
{
	LLVMContext& context = block->getContext();
	types::Type *type = getType();
	Value *rec = UndefValue::get(type->getLLVMType(context));
	unsigned idx = 0;

	IRBuilder<> builder(block);
	for (auto *expr : exprs) {
		Value *val = expr->codegen(base, block);
		builder.SetInsertPoint(block);  // recall: 'codegen' can change the block
		rec = builder.CreateInsertValue(rec, val, idx++);
	}

	return rec;
}

types::Type *RecordExpr::getType() const
{
	std::vector<types::Type *> types;
	for (auto *expr : exprs)
		types.push_back(expr->getType());
	return names.empty() ? types::RecordType::get(types) : types::RecordType::get(types, names);
}

RecordExpr *RecordExpr::clone(Generic *ref)
{
	std::vector<Expr *> exprsCloned;
	for (auto *expr : exprs)
		exprsCloned.push_back(expr->clone(ref));
	return new RecordExpr(exprsCloned, names);
}
