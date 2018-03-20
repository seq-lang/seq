#ifndef SEQ_RANGE_H
#define SEQ_RANGE_H

#include <cstdint>
#include "stage.h"

namespace seq {
	class Range : public Stage {
	private:
		seq_int_t from, to, step;
	public:
		Range(seq_int_t from, seq_int_t to, seq_int_t step);
		Range(seq_int_t from, seq_int_t to);
		explicit Range(seq_int_t to);
		void codegen(llvm::Module *module) override;
		static Range& make(seq_int_t from, seq_int_t to, seq_int_t step);
		static Range& make(seq_int_t from, seq_int_t to);
		static Range& make(seq_int_t to);
	};
}

#endif /* SEQ_RANGE_H */
