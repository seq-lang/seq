#ifndef SEQ_BASESTAGE_H
#define SEQ_BASESTAGE_H

#include "types.h"
#include "stage.h"

namespace seq {
	class BaseStage : public Stage {
	private:
		Stage *proxy;
		llvm::Value **deferredResult;
	public:
		BaseStage(types::Type *in,
		          types::Type *out,
		          Stage *proxy,
		          bool init=true);
		BaseStage(types::Type *in, types::Type *out, bool init=true);
		void codegen(llvm::Module *module) override;
		types::Type *getOutType() const override;
		void deferResult(llvm::Value **result);
		static BaseStage& make(types::Type *in,
		                       types::Type *out,
		                       Stage *proxy,
		                       bool init=true);
		static BaseStage& make(types::Type *in, types::Type *out, bool init=true);

		BaseStage *clone(types::RefType *ref) override;
	};
}

#endif /* SEQ_BASESTAGE_H */
