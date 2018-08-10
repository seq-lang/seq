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
			RecordType *contents;
			llvm::StructType *typeCached;
			mutable bool llvmTypeInProgress;
			explicit RefType(std::string name);
		public:
			RefType(RefType const&)=delete;
			void operator=(RefType const&)=delete;
			void setContents(RecordType *contents);
			std::string genericName() override;
			types::RefType *realize(std::vector<types::Type *> types) override;

			llvm::Value *memb(llvm::Value *self,
			                  const std::string& name,
			                  llvm::BasicBlock *block) override;

			Type *membType(const std::string& name) override;

			llvm::Value *setMemb(llvm::Value *self,
			                     const std::string& name,
			                     llvm::Value *val,
			                     llvm::BasicBlock *block) override;

			llvm::Value *staticMemb(const std::string& name, llvm::BasicBlock *block) override;

			Type *staticMembType(const std::string& name) override;

			llvm::Value *defaultValue(llvm::BasicBlock *block) override;

			llvm::Value *construct(BaseFunc *base,
			                       std::vector<llvm::Value *> args,
			                       llvm::BasicBlock *block) override;

			void initOps() override;
			void initFields() override;
			bool isAtomic() const override;
			bool is(types::Type *type) const override;
			Type *getBaseType(seq_int_t idx) const override;
			Type *getConstructType(std::vector<Type *> inTypes) override;
			llvm::Type *getLLVMType(llvm::LLVMContext& context) const override;
			seq_int_t size(llvm::Module *module) const override;
			llvm::Value *make(llvm::BasicBlock *block,
			                  std::vector<llvm::Value *> vals={});
			static RefType *get(std::string name);

			RefType *clone(Generic *ref) override;
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
			                  std::vector<llvm::Value *> args,
			                  llvm::BasicBlock *block) override;

			Type *getCallType(std::vector<Type *> inTypes) override;
			llvm::Value *make(llvm::Value *self, llvm::Value *func, llvm::BasicBlock *block);
			static MethodType *get(Type *self, FuncType *func);

			MethodType *clone(Generic *ref) override;
		};
	}
}

#endif /* SEQ_REF_H */
