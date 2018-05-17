#ifndef SEQ_VAREXPR_H
#define SEQ_VAREXPR_H

#include "var.h"
#include "cell.h"
#include "expr.h"

namespace seq {

	class VarExpr : public Expr {
	private:
		Var *var;
	public:
		explicit VarExpr(Var *var);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
	};

	class CellExpr : public Expr {
	private:
		Cell *cell;
	public:
		explicit CellExpr(Cell *cell);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
	};

}

#endif /* SEQ_VAREXPR_H */
