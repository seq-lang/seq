#ifndef SEQ_EXPRSTAGE_H
#define SEQ_EXPRSTAGE_H

#include "expr.h"
#include "cell.h"
#include "stage.h"
#include "basestage.h"
#include "patterns.h"

namespace seq {

	class ExprStage : public Stage {
	private:
		Expr *expr;
	public:
		explicit ExprStage(Expr *expr);
		void validate() override;
		void codegen(llvm::Module *module) override;
		static ExprStage& make(Expr *expr);
		ExprStage *clone(types::RefType *ref) override;
	};

	class CellStage : public Stage {
	private:
		Cell *cell;
	public:
		explicit CellStage(Cell *cell);
		void codegen(llvm::Module *module) override;
		static CellStage& make(Cell *cell);
		CellStage *clone(types::RefType *ref) override;
	};

	class AssignStage : public Stage {
	private:
		Cell *cell;
		Expr *value;
	public:
		AssignStage(Cell *cell, Expr *value);
		void codegen(llvm::Module *module) override;
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
		void codegen(llvm::Module *module) override;
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
		void codegen(llvm::Module *module) override;
		static AssignMemberStage& make(Expr *expr, std::string memb, Expr *value);
		static AssignMemberStage& make(Expr *expr, seq_int_t idx, Expr *value);
		AssignMemberStage *clone(types::RefType *ref) override;
	};

	class If : public Stage {
	private:
		std::vector<Expr *> conds;
		std::vector<BaseStage *> branches;
		bool elseAdded;
	public:
		If();
		BaseStage& addCond(Expr *cond);
		BaseStage& addElse();
		void codegen(llvm::Module *module) override;
		static If& make();
		If *clone(types::RefType *ref) override;
	};

	class Match : public Stage {
	private:
		Expr *value;
		std::vector<Pattern *> patterns;
		std::vector<BaseStage *> branches;
	public:
		Match();
		void setValue(Expr *value);
		BaseStage& addCase(Pattern *pattern);
		void codegen(llvm::Module *module) override;
		static Match& make();
		Match *clone(types::RefType *ref) override;
	};

	class While : public Stage {
	private:
		Expr *cond;
	public:
		explicit While(Expr *cond);
		void codegen(llvm::Module *module) override;
		static While& make(Expr *cond);
		While *clone(types::RefType *ref) override;
	};

	class Return : public Stage {
	private:
		Expr *expr;
	public:
		explicit Return(Expr *expr);
		void codegen(llvm::Module *module) override;
		static Return& make(Expr *expr);
		Return *clone(types::RefType *ref) override;
	};

	class Break : public Stage {
	public:
		Break();
		void codegen(llvm::Module *module) override;
		static Break& make();
		Break *clone(types::RefType *ref) override;
	};

	class Continue : public Stage {
	public:
		Continue();
		void codegen(llvm::Module *module) override;
		static Continue& make();
		Continue *clone(types::RefType *ref) override;
	};

}

#endif /* SEQ_EXPRSTAGE_H */
