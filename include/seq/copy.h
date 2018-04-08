#ifndef SEQ_COPY_H
#define SEQ_COPY_H

#include "stage.h"

namespace seq {
	class Copy : public Stage {
	private:
		llvm::Function *copyFunc;
	public:
		Copy();
		void codegen(llvm::Module *module) override;
		void finalize(llvm::Module *module, llvm::ExecutionEngine *eng) override;
		static Copy& make();
	};
}

#endif /* SEQ_COPY_H */
