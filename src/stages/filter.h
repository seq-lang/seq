#ifndef SEQ_FILTER_H
#define SEQ_FILTER_H

#include <string>
#include "stage.h"

namespace seq {
	class Filter : public Stage {
	private:
		llvm::Function *func;
		SeqPred op;
	public:
		explicit Filter(std::string name, SeqPred op);
		void codegen(llvm::Module *module, llvm::LLVMContext& context) override;
		void finalize(llvm::ExecutionEngine *eng) override;
		static Filter& make(std::string name, SeqPred op);
	};
}

#endif /* SEQ_FILTER_H */
