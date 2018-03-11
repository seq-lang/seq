#ifndef SEQ_RANGE_H
#define SEQ_RANGE_H

#include <cstdint>
#include "stage.h"

namespace seq {
	class Range : public Stage {
	private:
		uint32_t from, to, step;
	public:
		Range(uint32_t from, uint32_t to, uint32_t step);
		Range(uint32_t from, uint32_t to);
		explicit Range(uint32_t to);
		void codegen(llvm::Module *module, llvm::LLVMContext& context) override;
		static Range& make(uint32_t from, uint32_t to, uint32_t step);
		static Range& make(uint32_t from, uint32_t to);
		static Range& make(uint32_t to);
	};
}

#endif /* SEQ_RANGE_H */
