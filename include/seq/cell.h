#ifndef SEQ_CELL_H
#define SEQ_CELL_H

#include "types.h"
#include "expr.h"
#include "func.h"

namespace seq {
	class Cell {
	private:
		BaseFunc *base;
		Expr *init;
		llvm::Value *ptr;
	public:
		explicit Cell(BaseFunc *base, Expr *init);
		void codegen(llvm::BasicBlock *block);
		llvm::Value *load(llvm::BasicBlock *block);
		void store(llvm::Value *val, llvm::BasicBlock *block);
		types::Type *getType();

		Cell *clone(types::RefType *ref);
	};
}

#endif /* SEQ_CELL_H */
