#ifndef SEQ_ARRAY_H
#define SEQ_ARRAY_H

#include "types.h"

namespace seq {
	namespace types {

		class ArrayType : public Type {
		private:
			Type *base;
			llvm::StructType *arrStruct;
			explicit ArrayType(Type *base);
		public:
			ArrayType(ArrayType const&)=delete;
			void operator=(ArrayType const&)=delete;

			void callSerialize(ValMap outs,
                               llvm::BasicBlock *block,
                               std::string file) override;

			void finalizeSerialize(llvm::ExecutionEngine *eng) override;

			void callDeserialize(ValMap outs,
			                     llvm::BasicBlock *block,
			                     std::string file) override;

			void finalizeDeserialize(llvm::ExecutionEngine *eng) override;

			void callAlloc(ValMap outs,
			               seq_int_t count,
			               llvm::BasicBlock *block) override;

			void codegenLoad(ValMap outs,
			                 llvm::BasicBlock *block,
			                 llvm::Value *ptr,
			                 llvm::Value *idx) override;

			void codegenStore(ValMap outs,
			                  llvm::BasicBlock *block,
			                  llvm::Value *ptr,
			                  llvm::Value *idx) override;

			llvm::Type *getLLVMType(llvm::LLVMContext& context) override;
			llvm::Type *getLLVMArrayType(llvm::LLVMContext& context) override;
			seq_int_t size() const override;
			seq_int_t arraySize() const override;
			Type *getBaseType() const;
			ArrayType *of(Type& base) const;
			static ArrayType *get(Type *base);
			static ArrayType *get();
		};

	}
}

#endif /* SEQ_ARRAY_H */
