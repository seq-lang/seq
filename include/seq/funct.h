#ifndef SEQ_FUNCT_H
#define SEQ_FUNCT_H

#include "types.h"

namespace seq {
	namespace types {

		class FuncType : public Type {
		private:
			Type *inType;
			Type *outType;
			FuncType(Type *inType, Type *outType);
		public:
			FuncType(FuncType const&)=delete;
			void operator=(FuncType const&)=delete;

			void call(BaseFunc *base,
			          ValMap ins,
			          ValMap outs,
			          llvm::Value *fn,
			          llvm::BasicBlock *block) override;

			bool is(Type *type) const override;
			Type *getCallType(Type *inType) override;
			llvm::Type *getLLVMType(llvm::LLVMContext &context) const override;
			seq_int_t size(llvm::Module *module) const override;
			static FuncType *get(Type *inType, Type *outType);
		};

	}
}

#endif /* SEQ_FUNCT_H */
