#ifndef SEQ_SEQT_H
#define SEQ_SEQT_H

#include <iostream>
#include "types.h"

namespace seq {
	namespace types {
		extern "C" void printSeq(char *seq, seq_int_t len);

		class SeqType : public Type {
		private:
			SeqType();
		public:
			SeqType(SeqType const&)=delete;
			void operator=(SeqType const&)=delete;

			llvm::Type *getLLVMType(llvm::LLVMContext& context) override;
			void callPrint(std::shared_ptr<std::map<SeqData, llvm::Value *>> outs, llvm::BasicBlock *block) override;
			seq_int_t size() const override;
			static SeqType *get();
		};

		class MerType : public Type {
		private:
			seq_int_t k;
			MerType(seq_int_t k);
		public:
			MerType(MerType const&)=delete;
			void operator=(MerType const&)=delete;

			llvm::Type *getLLVMType(llvm::LLVMContext& context) override;
			seq_int_t size() const override;

			static MerType *get(seq_int_t k);
		};

	}
}

#endif /* SEQ_SEQT_H */
