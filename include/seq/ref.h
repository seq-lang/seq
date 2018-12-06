#ifndef SEQ_REF_H
#define SEQ_REF_H

#include <vector>
#include <map>
#include <utility>
#include "funct.h"
#include "generic.h"
#include "record.h"

namespace seq {
	class Func;

	namespace types {
		class GenericType;

		class RefType : public Type, public Generic {
		private:
			/*
			 * Whether the class is finished being constructed
			 */
			bool done;

			/*
			 * The base reference if generic
			 */
			RefType *root;

			/*
			 * Cache type-instantiations to avoid duplicate LLVM types
			 */
			mutable std::vector<std::pair<std::vector<types::Type *>, llvm::StructType *>> cache;

			/*
			 * These are for type-instantiations that happen _inside_ the class definition. We employ
			 * the somewhat hacky trick of returning a generic type and then realizing it with the
			 * correct reference type later.
			 */
			mutable std::vector<types::GenericType *> pendingRealizations;

			std::vector<std::pair<std::vector<types::Type *>, types::Type *>> realizationCache;
			RecordType *contents;
			explicit RefType(std::string name);

			llvm::Type *getStructPointerType(llvm::LLVMContext& context) const;
		public:
			RefType(RefType const&)=delete;
			void operator=(RefType const&)=delete;
			void setDone();
			void setContents(RecordType *contents);
			std::string getName() const override;
			std::string genericName() override;
			types::Type *realize(std::vector<types::Type *> types);
			std::vector<types::Type *> deduceTypesFromArgTypes(std::vector<types::Type *> argTypes);

			llvm::Value *memb(llvm::Value *self,
			                  const std::string& name,
			                  llvm::BasicBlock *block) override;

			Type *membType(const std::string& name) override;

			llvm::Value *setMemb(llvm::Value *self,
			                     const std::string& name,
			                     llvm::Value *val,
			                     llvm::BasicBlock *block) override;

			llvm::Value *defaultValue(llvm::BasicBlock *block) override;

			void initOps() override;
			void initFields() override;
			bool isAtomic() const override;
			bool is(types::Type *type) const override;
			unsigned numBaseTypes() const override;
			Type *getBaseType(unsigned idx) const override;
			llvm::Type *getLLVMType(llvm::LLVMContext& context) const override;
			size_t size(llvm::Module *module) const override;

			RefType *asRef() override;

			llvm::Value *make(llvm::BasicBlock *block,
			                  std::vector<llvm::Value *> vals={});
			static RefType *get(std::string name);

			types::RefType *clone(Generic *ref) override;
			void clearRealizationCache() override;
			types::Type *findCachedRealized(std::vector<types::Type *> types) const;
			void addCachedRealized(std::vector<types::Type *> types, Generic *x) override;

			static RefType *none();
		};

		class MethodType : public RecordType {
		private:
			Type *self;
			FuncType *func;
			MethodType(Type *self, FuncType *func);
		public:
			MethodType(MethodType const&)=delete;
			void operator=(MethodType const&)=delete;

			llvm::Value *call(BaseFunc *base,
			                  llvm::Value *self,
			                  const std::vector<llvm::Value *>& args,
			                  llvm::BasicBlock *block,
			                  llvm::BasicBlock *normal,
			                  llvm::BasicBlock *unwind) override;

			bool is(types::Type *type) const override;
			unsigned numBaseTypes() const override;
			Type *getBaseType(unsigned idx) const override;
			Type *getCallType(const std::vector<Type *>& inTypes) override;
			llvm::Value *make(llvm::Value *self, llvm::Value *func, llvm::BasicBlock *block);
			static MethodType *get(Type *self, FuncType *func);

			MethodType *clone(Generic *ref) override;
		};
	}
}

#endif /* SEQ_REF_H */
