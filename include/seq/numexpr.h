#ifndef SEQ_NUMEXPR_H
#define SEQ_NUMEXPR_H

#include "expr.h"

namespace seq {
	class IntExpr : public Expr {
	private:
		seq_int_t n;
	public:
		explicit IntExpr(seq_int_t n);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock *block) override;
	};
}

#endif /* SEQ_NUMEXPR_H */
