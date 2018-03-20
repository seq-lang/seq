#ifndef SEQ_SPLIT_H
#define SEQ_SPLIT_H

#include <cstdint>
#include "stage.h"

namespace seq {
	class Split : public Stage {
	private:
		seq_int_t k, step;
	public:
		Split(seq_int_t k, seq_int_t step);
		void codegen(llvm::Module *module) override;
		static Split& make(seq_int_t k, seq_int_t step);
	};
}

#endif /* SEQ_SPLIT_H */
