#ifndef SEQ_ARRAYEXPR_H
#define SEQ_ARRAYEXPR_H

#include "expr.h"

namespace seq {
	class ArrayExpr : public Expr {
	private:
		Expr *count;
	public:
		ArrayExpr(types::Type *type, Expr *count);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock *block) override;
	};
}

#endif /* SEQ_ARRAYEXPR_H */
