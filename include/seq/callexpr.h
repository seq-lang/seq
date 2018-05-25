#ifndef SEQ_CALLEXPR_H
#define SEQ_CALLEXPR_H

#include "expr.h"

namespace seq {
	class CallExpr : public Expr {
	private:
		Expr *func;
		Expr *arg;
	public:
		CallExpr(Expr *func, Expr *arg);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
	};
}

#endif /* SEQ_CALLEXPR_H */
