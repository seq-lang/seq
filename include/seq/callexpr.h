#ifndef SEQ_CALLEXPR_H
#define SEQ_CALLEXPR_H

#include <vector>
#include "expr.h"

namespace seq {
	class CallExpr : public Expr {
	private:
		mutable Expr *func;
		std::vector<Expr *> args;
	public:
		CallExpr(Expr *func, std::vector<Expr *> args);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		CallExpr *clone(Generic *ref) override;
	};
}

#endif /* SEQ_CALLEXPR_H */
