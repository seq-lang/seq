#ifndef SEQ_COUNT_H
#define SEQ_COUNT_H

#include "stage.h"

namespace seq {
	class Count : public Stage {
	public:
		Count();
		void codegen(llvm::Module *module, llvm::LLVMContext& context) override;
		static Count& make();
	};
}

#endif /* SEQ_COUNT_H */
