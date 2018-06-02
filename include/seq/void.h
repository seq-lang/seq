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

			llvm::Type *getFuncType(llvm::LLVMContext& context, Type *outType) override;

			llvm::Function *makeFuncOf(llvm::Module *module, Type *outType) override;

			llvm::Value *setFuncArgs(llvm::Function *func,
			                         llvm::BasicBlock *block) override;

			llvm::Value *callFuncOf(llvm::Value *func,
			                        llvm::Value *arg,
			                        llvm::BasicBlock *block) override;

			llvm::Value *loadFromAlloca(BaseFunc *base,
			                            llvm::Value *var,
			                            llvm::BasicBlock *block) override;

			llvm::Value *storeInAlloca(BaseFunc *base,
			                           llvm::Value *self,
			                           llvm::BasicBlock *block,
			                           bool storeDefault=false) override;

			llvm::Type *getLLVMType(llvm::LLVMContext &context) const override;
			static VoidType *get();
		};

	}
}

#endif /* SEQ_VOID_H */
