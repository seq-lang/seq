#ifndef SEQ_FILTER_H
#define SEQ_FILTER_H

#include <string>
#include "stage.h"

namespace seq {
	class Filter : public Stage {
	private:
		Func& func;
	public:
		explicit Filter(Func& func);
		void validate() override;
		void codegen(llvm::Module *module) override;
		void finalize(llvm::Module *module, llvm::ExecutionEngine *eng) override;
		static Filter& make(Func& func);
	};
}

#endif /* SEQ_FILTER_H */
