#ifndef SEQ_EXPRSTAGE_H
#define SEQ_EXPRSTAGE_H

#include "expr.h"
#include "cell.h"
#include "stage.h"
#include "patterns.h"

namespace seq {

	class Print : public Stage {
	private:
		Expr *expr;
	public:
		explicit Print(Expr *expr);
		void codegen(llvm::BasicBlock*& block) override;
		static Print& make(Expr *expr);
		Print *clone(types::RefType *ref) override;
	};

	class ExprStage : public Stage {
	private:
		Expr *expr;
	public:
		explicit ExprStage(Expr *expr);
		void codegen(llvm::BasicBlock*& block) override;
		static ExprStage& make(Expr *expr);
		ExprStage *clone(types::RefType *ref) override;
	};

	class CellStage : public Stage {
	private:
		Expr *init;
		Cell *var;
	public:
		explicit CellStage(Expr *init);
		Cell *getVar();
		void codegen(llvm::BasicBlock*& block) override;
		static CellStage& make(Expr *init);
		CellStage *clone(types::RefType *ref) override;
	};

	class AssignStage : public Stage {
	private:
		Cell *cell;
		Expr *value;
	public:
		AssignStage(Cell *cell, Expr *value);
		void codegen(llvm::BasicBlock*& block) override;
		static AssignStage& make(Cell *cell, Expr *value);
		AssignStage *clone(types::RefType *ref) override;
	};

	class AssignIndexStage : public Stage {
	private:
		Expr *array;
		Expr *idx;
		Expr *value;
	public:
		AssignIndexStage(Expr *array, Expr *idx, Expr *value);
		void codegen(llvm::BasicBlock*& block) override;
		static AssignIndexStage& make(Expr *array, Expr *idx, Expr *value);
		AssignIndexStage *clone(types::RefType *ref) override;
	};

	class AssignMemberStage : public Stage {
	private:
		Expr *expr;
		std::string memb;
		Expr *value;
	public:
		AssignMemberStage(Expr *expr, std::string memb, Expr *value);
		AssignMemberStage(Expr *expr, seq_int_t idx, Expr *value);
		void codegen(llvm::BasicBlock*& block) override;
		static AssignMemberStage& make(Expr *expr, std::string memb, Expr *value);
		static AssignMemberStage& make(Expr *expr, seq_int_t idx, Expr *value);
		AssignMemberStage *clone(types::RefType *ref) override;
	};

	class If : public Stage {
	private:
		std::vector<Expr *> conds;
		std::vector<Block *> branches;
		bool elseAdded;
	public:
		If();
		Block *addCond(Expr *cond);
		Block *addElse();
		void codegen(llvm::BasicBlock*& block) override;
		static If& make();
		If *clone(types::RefType *ref) override;
	};

	class Match : public Stage {
	private:
		Expr *value;
		std::vector<Pattern *> patterns;
		std::vector<Block *> branches;
	public:
		Match();
		void setValue(Expr *value);
		Block *addCase(Pattern *pattern);
		void codegen(llvm::BasicBlock*& block) override;
		static Match& make();
		Match *clone(types::RefType *ref) override;
	};

	class While : public Stage {
	private:
		Expr *cond;
		Block *scope;
	public:
		explicit While(Expr *cond);
		Block *getBlock();
		void codegen(llvm::BasicBlock*& block) override;
		static While& make(Expr *cond);
		While *clone(types::RefType *ref) override;
	};

	class For : public Stage {
	private:
		Expr *gen;
		Block *scope;
		Cell *var;
	public:
		explicit For(Expr *gen);
		Block *getBlock();
		Cell *getVar();
		void codegen(llvm::BasicBlock*& block) override;
		static For& make(Expr *gen);
		For *clone(types::RefType *ref) override;
	};

	class Return : public Stage {
	private:
		Expr *expr;
	public:
		explicit Return(Expr *expr);
		void codegen(llvm::BasicBlock*& block) override;
		static Return& make(Expr *expr);
		Return *clone(types::RefType *ref) override;
	};

	class Yield : public Stage {
	private:
		Expr *expr;
	public:
		explicit Yield(Expr *expr);
		void codegen(llvm::BasicBlock*& block) override;
		static Yield& make(Expr *expr);
		Yield *clone(types::RefType *ref) override;
	};

	class Break : public Stage {
	public:
		Break();
		void codegen(llvm::BasicBlock*& block) override;
		static Break& make();
		Break *clone(types::RefType *ref) override;
	};

	class Continue : public Stage {
	public:
		Continue();
		void codegen(llvm::BasicBlock*& block) override;
		static Continue& make();
		Continue *clone(types::RefType *ref) override;
	};

}

#endif /* SEQ_EXPRSTAGE_H */
