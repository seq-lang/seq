#ifndef SEQ_RETURN_H
#define SEQ_RETURN_H

#include "expr.h"
#include "stage.h"

namespace seq {
	class Return : public Stage {
	private:
		Expr *expr;
	public:
		explicit Return(Expr *expr);
		void codegen(llvm::Module *module) override;
		static Return& make(Expr *expr);
	};
}

#endif /* SEQ_RETURN_H */
