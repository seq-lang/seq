#ifndef SEQ_GETELEMEXPR_H
#define SEQ_GETELEMEXPR_H

#include "expr.h"

namespace seq {
	class GetElemExpr : public Expr {
	private:
		Expr *rec;
		seq_int_t idx;
	public:
		GetElemExpr(Expr *rec, seq_int_t idx);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
	};
}

#endif /* SEQ_GETELEMEXPR_H */
