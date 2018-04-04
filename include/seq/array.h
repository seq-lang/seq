#ifndef SEQ_ARRAY_H
#define SEQ_ARRAY_H

#include "types.h"

namespace seq {
	namespace types {

		class ArrayType : public Type {
		private:
			Type *baseType;
			llvm::StructType *arrStruct;
			explicit ArrayType(Type *baseType);
		public:
			ArrayType(ArrayType const&)=delete;
			void operator=(ArrayType const&)=delete;

			llvm::Function *makeFuncOf(llvm::Module *module, Type *outType) override;

			void setFuncArgs(llvm::Function *func,
			                 ValMap outs,
			                 llvm::BasicBlock *block) override;

			llvm::Value *callFuncOf(llvm::Function *func,
			                        ValMap outs,
			                        llvm::BasicBlock *block) override;

			llvm::Value *pack(BaseFunc *base,
			                  ValMap outs,
			                  llvm::BasicBlock *block) override;

			void unpack(BaseFunc *base,
			            llvm::Value *value,
			            ValMap outs,
			            llvm::BasicBlock *block) override;

			void callSerialize(BaseFunc *base,
			                   ValMap outs,
                               llvm::BasicBlock *block,
                               std::string file) override;

			void finalizeSerialize(llvm::ExecutionEngine *eng) override;

			void callDeserialize(BaseFunc *base,
			                     ValMap outs,
			                     llvm::BasicBlock *block,
			                     std::string file) override;

			void finalizeDeserialize(llvm::ExecutionEngine *eng) override;

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

			bool isChildOf(Type *type) override;
			llvm::Type *getLLVMType(llvm::LLVMContext& context) override;
			seq_int_t size() const override;
			Type *getBaseType() const;
			ArrayType& of(Type& baseType) const;
			static ArrayType *get(Type *baseType);
			static ArrayType *get();
		};

	}
}

#endif /* SEQ_ARRAY_H */
