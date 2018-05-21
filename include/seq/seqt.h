#ifndef SEQ_SEQT_H
#define SEQ_SEQT_H

#include <iostream>
#include "types.h"

namespace seq {
	namespace types {

		class BaseSeqType : public Type {
		protected:
			BaseSeqType(std::string name, SeqData key);
		public:
			BaseSeqType(BaseSeqType const&)=delete;
			void operator=(BaseSeqType const&)=delete;

			std::string copyFuncName() override { return "copyBaseSeq"; }
			std::string printFuncName() override { return "printBaseSeq"; }

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

			void callCopy(BaseFunc *base,
			              ValMap ins,
			              ValMap outs,
			              llvm::BasicBlock *block) override;

			void callPrint(BaseFunc *base,
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

			seq_int_t size(llvm::Module *module) const override;
		};

		class SeqType : public BaseSeqType {
		private:
			SeqType();
		public:
			llvm::Type *getLLVMType(llvm::LLVMContext& context) const override;
			static SeqType *get();
		};

		class StrType : public BaseSeqType {
		private:
			StrType();
		public:
			llvm::Type *getLLVMType(llvm::LLVMContext& context) const override;
			static StrType *get();
		};

	}
}

#endif /* SEQ_SEQT_H */
