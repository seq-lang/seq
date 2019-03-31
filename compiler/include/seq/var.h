#ifndef SEQ_VAR_H
#define SEQ_VAR_H

#include <stack>
#include "types.h"
#include "expr.h"
#include "func.h"

namespace seq {
	class Var {
	private:
		std::string name;
		types::Type *type;
		llvm::Value *ptr;
		llvm::Module *module;
		bool global;
		bool repl;
		std::stack<Var *> mapped;
		void allocaIfNeeded(BaseFunc *base);
	public:
		explicit Var(types::Type *type=nullptr);
		std::string getName();
		void setName(std::string name);
		bool isGlobal();
		void setGlobal();
		void setREPL();
		void mapTo(Var *other);
		void unmap();
		llvm::Value *getPtr(BaseFunc *base);
		llvm::Value *load(BaseFunc *base,
		                  llvm::BasicBlock *block,
		                  bool atomic=false);
		void store(BaseFunc *base,
		           llvm::Value *val,
		           llvm::BasicBlock *block,
		           bool atomic=false);
		void setType(types::Type *type);
		types::Type *getType();
		Var *clone(Generic *ref);
	};
}

#endif /* SEQ_VAR_H */
