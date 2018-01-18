#ifndef SEQ_SUBSTR_H
#define SEQ_SUBSTR_H

#include <cstdint>
#include "stage.h"

namespace seq {
	class Substr : public Stage {
	private:
		uint32_t start, len;
	public:
		Substr(uint32_t k, uint32_t step);
		void codegen(llvm::Module *module, llvm::LLVMContext& context) override;
		static Substr& make(uint32_t start, uint32_t len);
	};
}

#endif /* SEQ_SUBSTR_H */
