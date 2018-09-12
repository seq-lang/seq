#ifndef SEQ_RECORD_H
#define SEQ_RECORD_H

#include <vector>
#include <functional>
#include <initializer_list>
#include "types.h"

namespace seq {
	namespace types {

		class RecordType : public Type {
		protected:
			std::vector<Type *> types;
			std::vector<std::string> names;
			RecordType(std::vector<Type *> types, std::vector<std::string> names);
			RecordType(std::initializer_list<Type *> types);
		public:
			RecordType(RecordType const&)=delete;
			void operator=(RecordType const&)=delete;

			bool empty() const;
			std::vector<Type *> getTypes();

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

			llvm::Value *construct(BaseFunc *base,
			                       std::vector<llvm::Value *> args,
			                       llvm::BasicBlock *block) override;

			void initFields() override;

			bool isAtomic() const override;
			Type *getBaseType(seq_int_t idx) const override;
			Type *getConstructType(std::vector<Type *> inTypes) override;
			llvm::Type *getLLVMType(llvm::LLVMContext& context) const override;
			void addLLVMTypesToStruct(llvm::StructType *structType);
			seq_int_t size(llvm::Module *module) const override;
			RecordType& of(std::initializer_list<std::reference_wrapper<Type>> types) const;
			static RecordType *get(std::vector<Type *> types, std::vector<std::string> names={});
			static RecordType *get(std::initializer_list<Type *> types);

			RecordType *clone(Generic *ref) override;
		};

	}
}

#endif /* SEQ_RECORD_H */
