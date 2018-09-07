#ifndef SEQ_OPTIONAL_H
#define SEQ_OPTIONAL_H

#include "types.h"

namespace seq {
	namespace types {

		class OptionalType : public Type {
		private:
			Type *baseType;
			explicit OptionalType(Type *baseType);

			bool isRefOpt() const;
		public:
			OptionalType(OptionalType const&)=delete;
			void operator=(OptionalType const&)=delete;

			llvm::Value *defaultValue(llvm::BasicBlock *block) override;

			void initFields() override;

			bool isAtomic() const override;
			bool isGeneric(Type *type) const override;
			Type *getBaseType(seq_int_t idx) const override;
			llvm::Type *getLLVMType(llvm::LLVMContext& context) const override;
			seq_int_t size(llvm::Module *module) const override;
			llvm::Value *make(llvm::Value *val, llvm::BasicBlock *block);
			llvm::Value *has(llvm::Value *self, llvm::BasicBlock *block);
			llvm::Value *val(llvm::Value *self, llvm::BasicBlock *block);
			OptionalType& of(Type& baseType) const;
			static OptionalType *get(Type *baseType) noexcept;
			static OptionalType *get();
			OptionalType *clone(Generic *ref) override;
		};

	}
}

#endif /* SEQ_OPTIONAL_H */
