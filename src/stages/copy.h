#ifndef SEQ_COPY_H
#define SEQ_COPY_H

#include "stage.h"

namespace seq {
	class Copy : public Stage {
	public:
		Copy();
		void codegen(llvm::Module *module, llvm::LLVMContext& context) override;
		static Copy& make();
	};
}

#endif /* SEQ_COPY_H */
