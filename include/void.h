#ifndef SEQ_VOID_H
#define SEQ_VOID_H

#include "types.h"

namespace seq {
	namespace types {

		class VoidType : public Type {
		private:
			VoidType();
		public:
			VoidType(VoidType const &) = delete;
			void operator=(VoidType const &)= delete;

			virtual llvm::Function *makeFuncOf(llvm::Module *module,
			                                   ValMap outs,
			                                   Type *outType) override;

			virtual llvm::Value *callFuncOf(llvm::Function *func,
					                        ValMap outs,
			                                llvm::BasicBlock *block) override;

			virtual llvm::Value *pack(BaseFunc *base,
			                          ValMap outs,
			                          llvm::BasicBlock *block) override;

			virtual void unpack(BaseFunc *base,
			                    llvm::Value *value,
			                    ValMap outs,
			                    llvm::BasicBlock *block) override;

			llvm::Type *getLLVMType(llvm::LLVMContext &context) override;
			static VoidType *get();
		};

	}
}

#endif /* SEQ_VOID_H */
