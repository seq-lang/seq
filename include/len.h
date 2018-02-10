#ifndef SEQ_LEN_H
#define SEQ_LEN_H

#include "stage.h"

namespace seq {
	class Len : public Stage {
	public:
		Len();
		void codegen(llvm::Module *module, llvm::LLVMContext &context) override;
		static Len& make();
	};
}

#endif /* SEQ_LEN_H */
