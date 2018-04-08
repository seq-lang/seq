#ifndef SEQ_SEQT_H
#define SEQ_SEQT_H

#include <iostream>
#include "types.h"

namespace seq {
	namespace types {

		class SeqType : public Type {
		private:
			SeqType();
		public:
			SeqType(SeqType const&)=delete;
			void operator=(SeqType const&)=delete;

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

			llvm::Value *checkEq(BaseFunc *base,
			                     ValMap ins1,
			                     ValMap ins2,
			                     llvm::BasicBlock *block) override;

			void callPrint(BaseFunc *base,
			               ValMap outs,
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

			llvm::Type *getLLVMType(llvm::LLVMContext& context) override;
			seq_int_t size() const override;
			static SeqType *get();
		};

	}
}

#endif /* SEQ_SEQT_H */
