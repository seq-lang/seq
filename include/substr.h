#ifndef SEQ_SUBSTR_H
#define SEQ_SUBSTR_H

#include <cstdint>
#include "stage.h"

namespace seq {
	class Substr : public Stage {
	private:
		seq_int_t start, len;
	public:
		Substr(seq_int_t k, seq_int_t step);
		void codegen(llvm::Module *module, llvm::LLVMContext& context) override;
		static Substr& make(seq_int_t start, seq_int_t len);
	};
}

#endif /* SEQ_SUBSTR_H */
