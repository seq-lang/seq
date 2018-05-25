#ifndef SEQ_ARRAY_H
#define SEQ_ARRAY_H

#include "types.h"

namespace seq {
	namespace types {

		class ArrayType : public Type {
		private:
			Type *baseType;
			explicit ArrayType(Type *baseType);
		public:
			ArrayType(ArrayType const&)=delete;
			void operator=(ArrayType const&)=delete;

			std::string copyFuncName() override { return "copyArray"; }

			llvm::Type *getFuncType(llvm::LLVMContext& context, Type *outType) override;

			llvm::Function *makeFuncOf(llvm::Module *module, Type *outType) override;

			void setFuncArgs(llvm::Function *func,
			                 ValMap outs,
			                 llvm::BasicBlock *block) override;

			llvm::Value *callFuncOf(llvm::Value *func,
			                        ValMap outs,
			                        llvm::BasicBlock *block) override;

			llvm::Value *pack(BaseFunc *base,
			                  ValMap outs,
			                  llvm::BasicBlock *block) override;

			void unpack(BaseFunc *base,
			            llvm::Value *value,
			            ValMap outs,
			            llvm::BasicBlock *block) override;

			void callCopy(BaseFunc *base,
			              ValMap ins,
			              ValMap outs,
			              llvm::BasicBlock *block) override;

			void callSerialize(BaseFunc *base,
			                   ValMap outs,
			                   llvm::Value *fp,
			                   llvm::BasicBlock *block) override;

			void callDeserialize(BaseFunc *base,
			                     ValMap outs,
			                     llvm::Value *fp,
			                     llvm::BasicBlock *block) override;

			void codegenLoad(BaseFunc *base,
			                 ValMap outs,
			                 llvm::BasicBlock *block,
			                 llvm::Value *ptr,
			                 llvm::Value *idx) override;

			void codegenStore(BaseFunc *base,
			                  ValMap outs,
			                  llvm::BasicBlock *block,
			                  llvm::Value *ptr,
			                  llvm::Value *idx) override;

			void codegenIndexLoad(BaseFunc *base,
			                      ValMap outs,
			                      llvm::BasicBlock *block,
			                      llvm::Value *ptr,
			                      llvm::Value *idx) override;

			void codegenIndexStore(BaseFunc *base,
			                       ValMap outs,
			                       llvm::BasicBlock *block,
			                       llvm::Value *ptr,
			                       llvm::Value *idx) override;

			bool isGeneric(Type *type) const override;
			Type *getBaseType() const;
			Type *getBaseType(seq_int_t idx) const override;
			llvm::Type *getLLVMType(llvm::LLVMContext& context) const override;
			seq_int_t size(llvm::Module *module) const override;
			ArrayType& of(Type& baseType) const;
			static ArrayType *get(Type *baseType);
			static ArrayType *get();
		};

	}
}

#endif /* SEQ_ARRAY_H */
