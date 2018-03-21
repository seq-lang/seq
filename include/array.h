#ifndef SEQ_ARRAY_H
#define SEQ_ARRAY_H

#include "types.h"

namespace seq {
	namespace types {

		class ArrayType : public Type {
		private:
			Type *base;
			seq_int_t count;
			llvm::Function *mallocFunc;
			ArrayType(Type *base, seq_int_t count);
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
			               llvm::BasicBlock *block);

			void finalizeAlloc(llvm::ExecutionEngine *eng);

			llvm::Value *codegenLoad(llvm::BasicBlock *block,
                                     llvm::Value *ptr,
                                     llvm::Value *idx) override;

			void codegenStore(llvm::BasicBlock *block,
			                  llvm::Value *ptr,
			                  llvm::Value *idx,
			                  llvm::Value *val) override;

			llvm::Type *getLLVMType(llvm::LLVMContext& context) override;
			seq_int_t size() const override;
			Type *getBaseType() const;
			ArrayType *of(Type& base) const;
			ArrayType *of(Type& base, seq_int_t count) const;
			static ArrayType *get(Type *base, seq_int_t count);
		};

	}
}

#endif /* SEQ_ARRAY_H */
