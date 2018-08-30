#ifndef SEQ_LANG_H
#define SEQ_LANG_H

#include "expr.h"
#include "var.h"
#include "stmt.h"
#include "patterns.h"

namespace seq {

	class Print : public Stmt {
	private:
		Expr *expr;
	public:
		explicit Print(Expr *expr);
		void resolveTypes() override;
		void codegen0(llvm::BasicBlock*& block) override;
		Print *clone(Generic *ref) override;
	};

	class ExprStmt : public Stmt {
	private:
		Expr *expr;
	public:
		explicit ExprStmt(Expr *expr);
		void resolveTypes() override;
		void codegen0(llvm::BasicBlock*& block) override;
		ExprStmt *clone(Generic *ref) override;
	};

	class VarStmt : public Stmt {
	private:
		Expr *init;
		Var *var;
	public:
		explicit VarStmt(Expr *init);
		Var *getVar();
		void resolveTypes() override;
		void codegen0(llvm::BasicBlock*& block) override;
		VarStmt *clone(Generic *ref) override;
	};

	class FuncStmt : public Stmt {
	private:
		Func *func;
	public:
		explicit FuncStmt(Func *func);
		void resolveTypes() override;
		void codegen0(llvm::BasicBlock*& block) override;
		FuncStmt *clone(Generic *ref) override;
	};

	class Assign : public Stmt {
	private:
		Var *var;
		Expr *value;
	public:
		Assign(Var *var, Expr *value);
		void resolveTypes() override;
		void codegen0(llvm::BasicBlock*& block) override;
		Assign *clone(Generic *ref) override;
	};

	class AssignIndex : public Stmt {
	private:
		Expr *array;
		Expr *idx;
		Expr *value;
	public:
		AssignIndex(Expr *array, Expr *idx, Expr *value);
		void resolveTypes() override;
		void codegen0(llvm::BasicBlock*& block) override;
		AssignIndex *clone(Generic *ref) override;
	};

	class AssignMember : public Stmt {
	private:
		Expr *expr;
		std::string memb;
		Expr *value;
	public:
		AssignMember(Expr *expr, std::string memb, Expr *value);
		AssignMember(Expr *expr, seq_int_t idx, Expr *value);
		void resolveTypes() override;
		void codegen0(llvm::BasicBlock*& block) override;
		AssignMember *clone(Generic *ref) override;
	};

	class If : public Stmt {
	private:
		std::vector<Expr *> conds;
		std::vector<Block *> branches;
		bool elseAdded;
	public:
		If();
		Block *addCond(Expr *cond);
		Block *addElse();
		void resolveTypes() override;
		void codegen0(llvm::BasicBlock*& block) override;
		If *clone(Generic *ref) override;
	};

	class Match : public Stmt {
	private:
		Expr *value;
		std::vector<Pattern *> patterns;
		std::vector<Block *> branches;
	public:
		Match();
		void setValue(Expr *value);
		Block *addCase(Pattern *pattern);
		void resolveTypes() override;
		void codegen0(llvm::BasicBlock*& block) override;
		Match *clone(Generic *ref) override;
	};

	class While : public Stmt {
	private:
		Expr *cond;
		Block *scope;
	public:
		explicit While(Expr *cond);
		Block *getBlock();
		void resolveTypes() override;
		void codegen0(llvm::BasicBlock*& block) override;
		While *clone(Generic *ref) override;
	};

	class For : public Stmt {
	private:
		Expr *gen;
		Block *scope;
		Var *var;
	public:
		explicit For(Expr *gen);
		Block *getBlock();
		Var *getVar();
		void resolveTypes() override;
		void codegen0(llvm::BasicBlock*& block) override;
		For *clone(Generic *ref) override;
	};

	class Return : public Stmt {
	private:
		Expr *expr;
	public:
		explicit Return(Expr *expr);
		Expr *getExpr();
		void resolveTypes() override;
		void codegen0(llvm::BasicBlock*& block) override;
		Return *clone(Generic *ref) override;
	};

	class Yield : public Stmt {
	private:
		Expr *expr;
	public:
		explicit Yield(Expr *expr);
		Expr *getExpr();
		void resolveTypes() override;
		void codegen0(llvm::BasicBlock*& block) override;
		Yield *clone(Generic *ref) override;
	};

	class Break : public Stmt {
	public:
		Break();
		void codegen0(llvm::BasicBlock*& block) override;
		Break *clone(Generic *ref) override;
	};

	class Continue : public Stmt {
	public:
		Continue();
		void codegen0(llvm::BasicBlock*& block) override;
		Continue *clone(Generic *ref) override;
	};

}

#endif /* SEQ_LANG_H */
