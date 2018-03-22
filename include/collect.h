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
		void finalize(llvm::ExecutionEngine *eng) override;
		static Collect& make();
	};
}

#endif /* SEQ_COLLECT_H */
