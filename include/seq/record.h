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
			std::vector<std::string> names;
			RecordType(std::vector<Type *> types, std::vector<std::string> names);
			RecordType(std::initializer_list<Type *> types);
		public:
			RecordType(RecordType const&)=delete;
			void operator=(RecordType const&)=delete;

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

			void indexStore(BaseFunc *base,
			                llvm::Value *self,
			                llvm::Value *idx,
			                llvm::Value *val,
			                llvm::BasicBlock *block) override;

			llvm::Value *defaultValue(llvm::BasicBlock *block) override;

			void initFields() override;

			bool isGeneric(Type *type) const override;
			Type *getBaseType(seq_int_t idx) const override;
			llvm::Type *getLLVMType(llvm::LLVMContext& context) const override;
			seq_int_t size(llvm::Module *module) const override;
			RecordType& of(std::initializer_list<std::reference_wrapper<Type>> types) const;
			static RecordType *get(std::vector<Type *> types, std::vector<std::string> names={});
			static RecordType *get(std::initializer_list<Type *> types);
		};

	}
}

#endif /* SEQ_RECORD_H */
