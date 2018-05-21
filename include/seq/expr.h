#ifndef SEQ_EXPR_H
#define SEQ_EXPR_H

#include "types.h"

namespace seq {

	class Expr {
	private:
		types::Type *type;
	public:
		explicit Expr(types::Type *type);
		Expr();
		virtual llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block)=0;
		virtual types::Type *getType() const;
		virtual void ensure(types::Type *type);
	};

	class UOpExpr : public Expr {
	private:
		Op op;
		Expr *lhs;
	public:
		UOpExpr(Op op, Expr *lhs);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
	};

	class BOpExpr : public Expr {
	private:
		Op op;
		Expr *lhs;
		Expr *rhs;
	public:
		BOpExpr(Op op, Expr *lhs, Expr *rhs);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
	};

}

#endif /* SEQ_EXPR_H */
