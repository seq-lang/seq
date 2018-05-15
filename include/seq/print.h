#ifndef SEQ_PRINT_H
#define SEQ_PRINT_H

#include "opstage.h"

namespace seq {
	class Print : public Stage {
	public:
		Print();
		void validate() override;
		void codegen(llvm::Module *module) override;
		void finalize(llvm::Module *module, llvm::ExecutionEngine *eng) override;
		static Print& make();
	};
}

#endif /* SEQ_PRINT_H */
