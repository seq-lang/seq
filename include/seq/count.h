#ifndef SEQ_COUNT_H
#define SEQ_COUNT_H

#include "stage.h"

namespace seq {
	class Count : public Stage {
	public:
		Count();
		void codegen(llvm::Module *module) override;
		static Count& make();

		Count *clone(types::RefType *ref) override;
	};
}

#endif /* SEQ_COUNT_H */
