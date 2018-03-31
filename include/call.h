#ifndef SEQ_CALL_H
#define SEQ_CALL_H

#include "stage.h"

namespace seq {
	class Call : public Stage {
	private:
		BaseFunc& func;
	public:
		Call(BaseFunc& func);
		void codegen(llvm::Module *module) override;
		static Call& make(BaseFunc& func);
	};
}

#endif /* SEQ_CALL_H */
