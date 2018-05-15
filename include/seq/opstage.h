#ifndef SEQ_OP_H
#define SEQ_OP_H

#include <string>
#include "stage.h"

namespace seq {
	class OpStage : public Stage {
	private:
		llvm::Function *func;
		SeqOp op;
	public:
		OpStage(std::string name, SeqOp op);
		void codegen(llvm::Module *module) override;
		void finalize(llvm::Module *module, llvm::ExecutionEngine *eng) override;
		static OpStage& make(std::string name, SeqOp op);
	};
}

#endif /* SEQ_OP_H */
