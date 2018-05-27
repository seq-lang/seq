#ifndef SEQ_RANGE_H
#define SEQ_RANGE_H

#include <cstdint>
#include "expr.h"
#include "stage.h"

namespace seq {
	class Range : public LoopStage {
	private:
		Expr *from;
		Expr *to;
		Expr *step;
	public:
		Range(Expr *from, Expr *to, Expr *step);
		Range(Expr *from, Expr *to);
		explicit Range(Expr *to);
		Range(seq_int_t from, seq_int_t to, seq_int_t step);
		Range(seq_int_t from, seq_int_t to);
		explicit Range(seq_int_t to);
		void codegen(llvm::Module *module) override;
		static Range& make(Expr *from, Expr *to, Expr *step);
		static Range& make(Expr *from, Expr *to);
		static Range& make(Expr *to);
		static Range& make(seq_int_t from, seq_int_t to, seq_int_t step);
		static Range& make(seq_int_t from, seq_int_t to);
		static Range& make(seq_int_t to);
	};
}

#endif /* SEQ_RANGE_H */
