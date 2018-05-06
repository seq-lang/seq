#ifndef SEQ_EXPR_H
#define SEQ_EXPR_H

#include "types.h"

namespace seq {
	class Expr {
	private:
		types::Type *type;
	public:
		explicit Expr(types::Type *type);
		virtual llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock *block)=0;
		virtual types::Type *getType() const;
		virtual void ensure(types::Type *type);
	};
}

#endif /* SEQ_EXPR_H */
