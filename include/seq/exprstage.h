#ifndef SEQ_EXPRSTAGE_H
#define SEQ_EXPRSTAGE_H

#include "expr.h"
#include "stage.h"

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
}

#endif /* SEQ_EXPRSTAGE_H */
