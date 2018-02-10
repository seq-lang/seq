#ifndef SEQ_PRINT_H
#define SEQ_PRINT_H

#include "op.h"

namespace seq {
	class Print : public Stage {
	public:
		Print();
		void validate() override;
		void codegen(llvm::Module *module, llvm::LLVMContext& context) override;
		void finalize(llvm::ExecutionEngine *eng) override;
		static Print& make();
	};
}

#endif /* SEQ_PRINT_H */
