#ifndef SEQ_BASESTAGE_H
#define SEQ_BASESTAGE_H

#include "types.h"
#include "stage.h"

namespace seq {
	class BaseStage : public Stage {
	public:
		BaseStage(types::Type *in, types::Type *out);
		void codegen(llvm::Module *module, llvm::LLVMContext& context) override;
		static BaseStage& make(types::Type *in, types::Type *out);
	};
}

#endif /* SEQ_BASESTAGE_H */
