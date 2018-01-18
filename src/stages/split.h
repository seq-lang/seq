#ifndef SEQ_SPLIT_H
#define SEQ_SPLIT_H

#include <cstdint>
#include "stage.h"

namespace seq {
	class Split : public Stage {
	private:
		uint32_t k, step;
	public:
		Split(uint32_t k, uint32_t step);
		void codegen(llvm::Module *module, llvm::LLVMContext& context) override;
		static Split& make(uint32_t k, uint32_t step);
	};

}

#endif /* SEQ_SPLIT_H */
