#ifndef SEQ_CALL_H
#define SEQ_CALL_H

#include "stage.h"

namespace seq {
	class Call : public Stage {
	private:
		Func& func;
	public:
		explicit Call(Func& func);
		void validate() override;
		void codegen(llvm::Module *module) override;
		void finalize(llvm::Module *module, llvm::ExecutionEngine *eng) override;
		static Call& make(Func& func);

		Call *clone(types::RefType *ref) override;
	};
}

#endif /* SEQ_CALL_H */
