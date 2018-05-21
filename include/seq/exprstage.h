#ifndef SEQ_EXPRSTAGE_H
#define SEQ_EXPRSTAGE_H

#include "expr.h"
#include "cell.h"
#include "stage.h"
#include "basestage.h"

namespace seq {

	class ExprStage : public Stage {
	private:
		Expr *expr;
	public:
		explicit ExprStage(Expr *expr);
		void validate() override;
		void codegen(llvm::Module *module) override;
		static ExprStage& make(Expr *expr);
	};

	class CellStage : public Stage {
	private:
		Cell *cell;
	public:
		explicit CellStage(Cell *cell);
		void codegen(llvm::Module *module) override;
		static CellStage& make(Cell *cell);
	};

	class AssignStage : public Stage {
	private:
		Cell *cell;
		Expr *value;
	public:
		explicit AssignStage(Cell *cell, Expr *value);
		void codegen(llvm::Module *module) override;
		static AssignStage& make(Cell *cell, Expr *value);
	};

	class AssignIndexStage : public Stage {
	private:
		Expr *array;
		Expr *idx;
		Expr *value;
	public:
		explicit AssignIndexStage(Expr *array, Expr *idx, Expr *value);
		void codegen(llvm::Module *module) override;
		static AssignIndexStage& make(Expr *array, Expr *idx, Expr *value);
	};

	class AssignMemberStage : public Stage {
	private:
		Cell *cell;
		seq_int_t idx;
		Expr *value;
	public:
		explicit AssignMemberStage(Cell *cell, seq_int_t idx, Expr *value);
		void codegen(llvm::Module *module) override;
		static AssignMemberStage& make(Cell *cell, seq_int_t idx, Expr *value);
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
	};

}

#endif /* SEQ_EXPRSTAGE_H */
