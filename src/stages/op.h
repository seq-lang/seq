#ifndef SEQ_OP_H
#define SEQ_OP_H

#include <string>
#include "stage.h"

namespace seq {
	class Op : public Stage {
	private:
		llvm::Function *func;
		SeqOp op;
	public:
		explicit Op(std::string name, SeqOp op);
		void codegen(llvm::Module *module, llvm::LLVMContext& context) override;
		void finalize(llvm::ExecutionEngine *eng) override;
		static Op& make(std::string name, SeqOp op);
	};
}

#endif /* SEQ_OP_H */
