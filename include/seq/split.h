#ifndef SEQ_SPLIT_H
#define SEQ_SPLIT_H

#include <cstdint>
#include "expr.h"
#include "stage.h"

namespace seq {
	class Split : public Stage {
	private:
		Expr *k;
		Expr *step;
	public:
		Split(Expr *k, Expr *step);
		Split(seq_int_t k, seq_int_t step);
		void codegen(llvm::Module *module) override;
		static Split& make(Expr *k, Expr *step);
		static Split& make(seq_int_t k, seq_int_t step);

		Split *clone(types::RefType *ref) override;
	};
}

#endif /* SEQ_SPLIT_H */
