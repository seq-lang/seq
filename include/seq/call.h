#ifndef SEQ_CALL_H
#define SEQ_CALL_H

#include <vector>
#include "stage.h"

namespace seq {
	class Call : public Stage {
	private:
		Func& func;
	public:
		explicit Call(Func& func);
		void codegen(llvm::Module *module) override;
		void finalize(llvm::Module *module, llvm::ExecutionEngine *eng) override;
		static Call& make(Func& func);
	};

	class MultiCall : public Stage {
	private:
		std::vector<Func *> funcs;
	public:
		explicit MultiCall(std::vector<Func *> funcs);
		void codegen(llvm::Module *module) override;
		void finalize(llvm::Module *module, llvm::ExecutionEngine *eng) override;
		static MultiCall& make(std::vector<Func *> funcs);
	};
}

#endif /* SEQ_CALL_H */
