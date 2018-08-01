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

			llvm::Value *eq(BaseFunc *base,
			                llvm::Value *self,
			                llvm::Value *other,
			                llvm::BasicBlock *block) override;

			llvm::Value *copy(BaseFunc *base,
			                  llvm::Value *self,
			                  llvm::BasicBlock *block) override;

			void print(BaseFunc *base,
			           llvm::Value *self,
			           llvm::BasicBlock *block) override;

			void serialize(BaseFunc *base,
			               llvm::Value *self,
			               llvm::Value *fp,
			               llvm::BasicBlock *block) override;

			llvm::Value *deserialize(BaseFunc *base,
			                         llvm::Value *fp,
			                         llvm::BasicBlock *block) override;

			llvm::Value *indexLoad(BaseFunc *base,
			                       llvm::Value *self,
			                       llvm::Value *idx,
			                       llvm::BasicBlock *block) override;

			llvm::Value *indexSlice(BaseFunc *base,
			                        llvm::Value *self,
			                        llvm::Value *from,
			                        llvm::Value *to,
			                        llvm::BasicBlock *block) override;

			llvm::Value *indexSliceNoFrom(BaseFunc *base,
			                              llvm::Value *self,
			                              llvm::Value *to,
			                              llvm::BasicBlock *block) override;

			llvm::Value *indexSliceNoTo(BaseFunc *base,
			                            llvm::Value *self,
			                            llvm::Value *from,
			                            llvm::BasicBlock *block) override;

			llvm::Value *defaultValue(llvm::BasicBlock *block) override;

			void initFields() override;

			bool isAtomic() const override;
			llvm::Type *getLLVMType(llvm::LLVMContext& context) const override;
			seq_int_t size(llvm::Module *module) const override;

			virtual llvm::Value *make(llvm::Value *ptr, llvm::Value *len, llvm::BasicBlock *block)=0;
		};

		class SeqType : public BaseSeqType {
		private:
			SeqType();
		public:
			llvm::Value *memb(llvm::Value *self,
			                  const std::string& name,
			                  llvm::BasicBlock *block) override;

			llvm::Value *setMemb(llvm::Value *self,
			                     const std::string& name,
			                     llvm::Value *val,
			                     llvm::BasicBlock *block) override;

			void initOps() override;
			Type *indexType() const override;
			llvm::Value *make(llvm::Value *ptr, llvm::Value *len, llvm::BasicBlock *block) override;
			static SeqType *get();
		};

		class StrType : public BaseSeqType {
		private:
			StrType();
		public:
			llvm::Value *memb(llvm::Value *self,
			                  const std::string& name,
			                  llvm::BasicBlock *block) override;

			llvm::Value *setMemb(llvm::Value *self,
			                     const std::string& name,
			                     llvm::Value *val,
			                     llvm::BasicBlock *block) override;

			void initOps() override;
			Type *indexType() const override;
			llvm::Value *make(llvm::Value *ptr, llvm::Value *len, llvm::BasicBlock *block) override;
			static StrType *get();
		};

	}
}

#endif /* SEQ_SEQT_H */
