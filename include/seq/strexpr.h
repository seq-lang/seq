#ifndef SEQ_STREXPR_H
#define SEQ_STREXPR_H

#include "expr.h"

namespace seq {
	class StrExpr : public Expr {
	private:
		std::string s;
	public:
		explicit StrExpr(std::string s);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
	};
}

#endif /* SEQ_STREXPR_H */
