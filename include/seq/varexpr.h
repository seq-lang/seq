#ifndef SEQ_VAREXPR_H
#define SEQ_VAREXPR_H

#include "cell.h"
#include "func.h"
#include "expr.h"

namespace seq {

	class CellExpr : public Expr {
	private:
		Cell *cell;
	public:
		explicit CellExpr(Cell *cell);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		CellExpr *clone(types::RefType *ref) override;
	};

	class FuncExpr : public Expr {
	private:
		BaseFunc *func;
	public:
		explicit FuncExpr(BaseFunc *func);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		FuncExpr *clone(types::RefType *ref) override;
	};

}

#endif /* SEQ_VAREXPR_H */
