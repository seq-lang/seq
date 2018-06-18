#ifndef SEQ_COLLECT_H
#define SEQ_COLLECT_H

#include "stage.h"

namespace seq {
	class Collect : public Stage {
	private:
		llvm::Function *appendFunc;
	public:
		Collect();
		void validate() override;
		void codegen(llvm::Module *module) override;
		void finalize(llvm::Module *module, llvm::ExecutionEngine *eng) override;
		static Collect& make();

		Collect *clone(types::RefType *ref) override;
	};
}

#endif /* SEQ_COLLECT_H */
