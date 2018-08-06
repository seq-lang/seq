#ifndef SEQ_CELL_H
#define SEQ_CELL_H

#include "types.h"
#include "expr.h"
#include "func.h"

namespace seq {
	class Cell {
	private:
		types::Type *type;
		llvm::Value *ptr;

		void allocaIfNeeded(BaseFunc *base);
	public:
		explicit Cell(types::Type *type=nullptr);
		llvm::Value *load(BaseFunc *base, llvm::BasicBlock *block);
		void store(BaseFunc *base, llvm::Value *val, llvm::BasicBlock *block);
		void setType(types::Type *type);
		types::Type *getType();
		Cell *clone(types::RefType *ref);
	};
}

#endif /* SEQ_CELL_H */
