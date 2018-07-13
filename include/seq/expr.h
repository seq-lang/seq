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
		virtual Expr *clone(types::RefType *ref);
	};

	class UOpExpr : public Expr {
	private:
		Op op;
		Expr *lhs;
	public:
		UOpExpr(Op op, Expr *lhs);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		UOpExpr *clone(types::RefType *ref) override;
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
		BOpExpr *clone(types::RefType *ref) override;
	};

	class CondExpr : public Expr {
	private:
		Expr *cond;
		Expr *ifTrue;
		Expr *ifFalse;
	public:
		CondExpr(Expr *cond, Expr *ifTrue, Expr *ifFalse);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		CondExpr *clone(types::RefType *ref) override;
	};

	class ConstructExpr : public Expr {
	private:
		types::Type *type;
		std::vector<Expr *> args;
	public:
		ConstructExpr(types::Type *type, std::vector<Expr *> args);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		ConstructExpr *clone(types::RefType *ref) override;
	};

	class OptExpr : public Expr {
	private:
		Expr *val;
	public:
		explicit OptExpr(Expr *val);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		OptExpr *clone(types::RefType *ref) override;
	};

	class DefaultExpr : public Expr {
	public:
		explicit DefaultExpr(types::Type *type);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		DefaultExpr *clone(types::RefType *ref) override;
	};

}

#endif /* SEQ_EXPR_H */
