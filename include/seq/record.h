#ifndef SEQ_RECORD_H
#define SEQ_RECORD_H

#include <vector>
#include <functional>
#include <initializer_list>
#include "types.h"

namespace seq {
	namespace types {

		class RecordType : public Type {
		private:
			std::vector<Type *> types;
			explicit RecordType(std::vector<Type *> types);
			RecordType(std::initializer_list<Type *> types);
		public:
			RecordType(RecordType const&)=delete;
			void operator=(RecordType const&)=delete;

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
			Type *getBaseType(seq_int_t idx) const override;
			llvm::Type *getLLVMType(llvm::LLVMContext& context) const override;
			seq_int_t size(llvm::Module *module) const override;
			RecordType& of(std::initializer_list<std::reference_wrapper<Type>> types) const;
			static RecordType *get(std::vector<Type *> types);
			static RecordType *get(std::initializer_list<Type *> types);
		};

	}
}

#endif /* SEQ_RECORD_H */
