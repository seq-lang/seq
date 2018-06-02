#ifndef SEQ_BASESTAGE_H
#define SEQ_BASESTAGE_H

#include "types.h"
#include "stage.h"

namespace seq {
	class BaseStage : public InitStage {
	private:
		Stage *proxy;
		llvm::Value **deferredResult;
	public:
		BaseStage(types::Type *in, types::Type *out, Stage *proxy);
		BaseStage(types::Type *in, types::Type *out);
		void codegen(llvm::Module *module) override;
		types::Type *getOutType() const override;
		void deferResult(llvm::Value **result);
		static BaseStage& make(types::Type *in, types::Type *out, Stage *proxy);
		static BaseStage& make(types::Type *in, types::Type *out);
	};
}

#endif /* SEQ_BASESTAGE_H */
