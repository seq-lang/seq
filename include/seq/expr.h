#ifndef SEQ_EXPR_H
#define SEQ_EXPR_H

#include "patterns.h"
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
		virtual Expr *clone(Generic *ref);
	};

	class UOpExpr : public Expr {
	private:
		Op op;
		Expr *lhs;
	public:
		UOpExpr(Op op, Expr *lhs);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		UOpExpr *clone(Generic *ref) override;
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
		BOpExpr *clone(Generic *ref) override;
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
		CondExpr *clone(Generic *ref) override;
	};

	class MatchExpr : public Expr {
	private:
		Expr *value;
		std::vector<Pattern *> patterns;
		std::vector<Expr *> exprs;
	public:
		MatchExpr();
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		MatchExpr *clone(Generic *ref) override;
		void setValue(Expr *value);
		void addCase(Pattern *pattern, Expr *expr);
	};

	class ConstructExpr : public Expr {
	private:
		types::Type *type;
		std::vector<Expr *> args;
	public:
		ConstructExpr(types::Type *type, std::vector<Expr *> args);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		ConstructExpr *clone(Generic *ref) override;
	};

	class OptExpr : public Expr {
	private:
		Expr *val;
	public:
		explicit OptExpr(Expr *val);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		OptExpr *clone(Generic *ref) override;
	};

	class DefaultExpr : public Expr {
	public:
		explicit DefaultExpr(types::Type *type);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		DefaultExpr *clone(Generic *ref) override;
	};

}

#endif /* SEQ_EXPR_H */
