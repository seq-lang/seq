#ifndef SEQ_FUNCT_H
#define SEQ_FUNCT_H

#include "types.h"

namespace seq {
	namespace types {

		class FuncType : public Type {
		private:
			std::vector<Type *> inTypes;
			Type *outType;
			FuncType(std::vector<Type *> inTypes, Type *outType);
		public:
			FuncType(FuncType const&)=delete;
			void operator=(FuncType const&)=delete;

			llvm::Value *call(BaseFunc *base,
			                  llvm::Value *self,
			                  std::vector<llvm::Value *> args,
			                  llvm::BasicBlock *block) override;

			llvm::Value *defaultValue(llvm::BasicBlock *block) override;

			bool is(Type *type) const override;
			Type *getCallType(std::vector<Type *> inTypes) override;
			llvm::Type *getLLVMType(llvm::LLVMContext &context) const override;
			seq_int_t size(llvm::Module *module) const override;
			static FuncType *get(std::vector<Type *> inTypes, Type *outType);

			FuncType *clone(Generic *ref) override;
		};

		// Generator types really represent generator handles in LLVM
		class GenType : public Type {
		private:
			Type *outType;
			explicit GenType(Type *outType);
		public:
			GenType(GenType const&)=delete;
			void operator=(FuncType const&)=delete;

			llvm::Value *defaultValue(llvm::BasicBlock *block) override;
			llvm::Value *done(llvm::Value *self, llvm::BasicBlock *block);
			void resume(llvm::Value *self, llvm::BasicBlock *block);
			llvm::Value *promise(llvm::Value *self, llvm::BasicBlock *block);
			void destroy(llvm::Value *self, llvm::BasicBlock *block);

			Type *getBaseType(seq_int_t idx) const override;
			bool is(Type *type) const override;
			bool isGeneric(Type *type) const override;
			llvm::Type *getLLVMType(llvm::LLVMContext &context) const override;
			seq_int_t size(llvm::Module *module) const override;
			static GenType *get(Type *outType);
			static GenType *get();

			GenType *clone(Generic *ref) override;
		};

	}
}

#endif /* SEQ_FUNCT_H */
