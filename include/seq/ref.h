#ifndef SEQ_REF_H
#define SEQ_REF_H

#include <vector>
#include <map>
#include "funct.h"
#include "record.h"

namespace seq {
	class Func;

	namespace types {

		class RefType : public Type {
		private:
			RecordType *contents;
			std::map<std::string, Func *> methods;
			llvm::StructType *typeCached;
			explicit RefType(std::string name);
		public:
			RefType(RefType const&)=delete;
			void operator=(RefType const&)=delete;

			RecordType *getContents();
			void setContents(RecordType *contents);
			void addMethod(std::string name, Func *func);

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
			Type *getBaseType(seq_int_t idx) const override;
			llvm::Type *getLLVMType(llvm::LLVMContext& context) const override;
			seq_int_t size(llvm::Module *module) const override;
			llvm::Value *make(llvm::BasicBlock *block) const;
			static RefType *get(std::string name);
		};

		class MethodType : public RecordType {
		private:
			RefType *self;
			FuncType *func;
			explicit MethodType(RefType *self, FuncType *func);
		public:
			MethodType(MethodType const&)=delete;
			void operator=(MethodType const&)=delete;

			llvm::Value *call(BaseFunc *base,
			                  llvm::Value *self,
			                  std::vector<llvm::Value *> args,
			                  llvm::BasicBlock *block) override;

			Type *getCallType(std::vector<Type *> inTypes) override;
			llvm::Value *make(llvm::Value *self, llvm::Value *func, llvm::BasicBlock *block);
			static MethodType *get(RefType *self, FuncType *func);
		};

	}
}

#endif /* SEQ_REF_H */
