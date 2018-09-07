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

			llvm::Value *loadFromAlloca(BaseFunc *base,
			                            llvm::Value *var,
			                            llvm::BasicBlock *block) override;

			llvm::Value *storeInAlloca(BaseFunc *base,
			                           llvm::Value *self,
			                           llvm::BasicBlock *block,
			                           bool storeDefault) override;

			llvm::Type *getLLVMType(llvm::LLVMContext &context) const override;
			static VoidType *get() noexcept;
		};

	}
}

#endif /* SEQ_VOID_H */
