#ifndef SEQ_LEN_H
#define SEQ_LEN_H

#include "stage.h"

namespace seq {
	class Len : public Stage {
	public:
		Len();
		void codegen(llvm::Module *module) override;
		static Len& make();

		Len *clone(types::RefType *ref) override;
	};
}

#endif /* SEQ_LEN_H */
