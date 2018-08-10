#ifndef SEQ_VAR_H
#define SEQ_VAR_H

#include "types.h"
#include "expr.h"
#include "func.h"

namespace seq {
	class Var {
	private:
		types::Type *type;
		llvm::Value *ptr;
		bool global;
		void allocaIfNeeded(BaseFunc *base);
	public:
		explicit Var(types::Type *type=nullptr);
		bool isGlobal();
		void setGlobal();
		llvm::Value *load(BaseFunc *base, llvm::BasicBlock *block);
		void store(BaseFunc *base, llvm::Value *val, llvm::BasicBlock *block);
		void setType(types::Type *type);
		types::Type *getType();
		Var *clone(Generic *ref);
	};
}

#endif /* SEQ_VAR_H */
