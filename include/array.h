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

			void callSerialize(Seq *base,
			                   ValMap outs,
                               llvm::BasicBlock *block,
                               std::string file) override;

			void finalizeSerialize(llvm::ExecutionEngine *eng) override;

			void callDeserialize(Seq *base,
			                     ValMap outs,
			                     llvm::BasicBlock *block,
			                     std::string file) override;

			void finalizeDeserialize(llvm::ExecutionEngine *eng) override;

			void codegenLoad(Seq *base,
			                 ValMap outs,
			                 llvm::BasicBlock *block,
			                 llvm::Value *ptr,
			                 llvm::Value *idx) override;

			void codegenStore(Seq *base,
			                  ValMap outs,
			                  llvm::BasicBlock *block,
			                  llvm::Value *ptr,
			                  llvm::Value *idx) override;

			llvm::Type *getLLVMType(llvm::LLVMContext& context) override;
			llvm::Type *getLLVMArrayType(llvm::LLVMContext& context) override;
			seq_int_t size() const override;
			seq_int_t arraySize() const override;
			Type *getBaseType() const;
			ArrayType& of(Type& baseType) const;
			static ArrayType *get(Type *baseType);
			static ArrayType *get();
		};

	}
}

#endif /* SEQ_ARRAY_H */
