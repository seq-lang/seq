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

			llvm::Value *defaultValue(llvm::BasicBlock *block) override;

			void initOps() override;
			void initFields() override;
			bool isAtomic() const override;
			bool is(Type *type) const override;
			unsigned numBaseTypes() const override;
			Type *getBaseType(unsigned idx) const override;
			llvm::Type *getLLVMType(llvm::LLVMContext& context) const override;
			void addLLVMTypesToStruct(llvm::StructType *structType);
			size_t size(llvm::Module *module) const override;

			RecordType *asRec() override;

			RecordType& of(std::initializer_list<std::reference_wrapper<Type>> types) const;
			static RecordType *get(std::vector<Type *> types, std::vector<std::string> names={});
			static RecordType *get(std::initializer_list<Type *> types);

			RecordType *clone(Generic *ref) override;
		};

	}
}

#endif /* SEQ_RECORD_H */
