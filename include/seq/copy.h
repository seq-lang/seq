#ifndef SEQ_COPY_H
#define SEQ_COPY_H

#include "stage.h"

namespace seq {
	class Copy : public Stage {
	public:
		Copy();
		void codegen(llvm::Module *module) override;
		void validate() override;
		void finalize(llvm::Module *module, llvm::ExecutionEngine *eng) override;
		static Copy& make();

		Copy *clone(types::RefType *ref) override;
	};
}

#endif /* SEQ_COPY_H */
