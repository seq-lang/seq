#ifndef SEQ_SUBSTR_H
#define SEQ_SUBSTR_H

#include <cstdint>
#include "expr.h"
#include "stage.h"

namespace seq {
	class Substr : public Stage {
	private:
		Expr *start;
		Expr *len;
	public:
		Substr(Expr *start, Expr *len);
		Substr(seq_int_t k, seq_int_t step);
		void codegen(llvm::Module *module) override;
		static Substr& make(Expr *start, Expr *len);
		static Substr& make(seq_int_t start, seq_int_t len);
	};
}

#endif /* SEQ_SUBSTR_H */
