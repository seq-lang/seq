#ifndef SEQ_RECORDEXPR_H
#define SEQ_RECORDEXPR_H

#include <vector>
#include "expr.h"

namespace seq {
	class RecordExpr : public Expr {
	private:
		std::vector<Expr *> exprs;
		std::vector<std::string> names;
	public:
		explicit RecordExpr(std::vector<Expr *> exprs, std::vector<std::string> names={});
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		RecordExpr *clone(Generic *ref) override;
	};
}

#endif /* SEQ_RECORDEXPR_H */
