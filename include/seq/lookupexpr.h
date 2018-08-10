#ifndef SEQ_LOOKUPEXPR_H
#define SEQ_LOOKUPEXPR_H

#include "expr.h"

namespace seq {

	class ArrayLookupExpr : public Expr {
	private:
		Expr *arr;
		Expr *idx;
	public:
		ArrayLookupExpr(Expr *arr, Expr *idx);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		ArrayLookupExpr *clone(Generic *ref) override;
	};

	class ArraySliceExpr : public Expr {
	private:
		Expr *arr;
		Expr *from;
		Expr *to;
	public:
		ArraySliceExpr(Expr *arr, Expr *from, Expr *to);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		ArraySliceExpr *clone(Generic *ref) override;
	};

}

#endif /* SEQ_LOOKUPEXPR_H */
