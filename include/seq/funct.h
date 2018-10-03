#ifndef SEQ_FUNCT_H
#define SEQ_FUNCT_H

#include "types.h"
#include "record.h"

namespace seq {
	namespace types {

		class FuncType : public Type {
		private:
			std::vector<Type *> inTypes;
			Type *outType;
			FuncType(std::vector<Type *> inTypes, Type *outType);
		public:
			FuncType(FuncType const&)=delete;
			void operator=(FuncType const&)=delete;

			unsigned argCount() const;

			llvm::Value *call(BaseFunc *base,
			                  llvm::Value *self,
			                  const std::vector<llvm::Value *>& args,
			                  llvm::BasicBlock *block) override;

			llvm::Value *defaultValue(llvm::BasicBlock *block) override;

			bool is(Type *type) const override;
			unsigned numBaseTypes() const override;
			Type *getBaseType(unsigned idx) const override;
			Type *getCallType(const std::vector<Type *>& inTypes) override;
			llvm::Type *getLLVMType(llvm::LLVMContext &context) const override;
			seq_int_t size(llvm::Module *module) const override;
			static FuncType *get(std::vector<Type *> inTypes, Type *outType);

			FuncType *clone(Generic *ref) override;
		};

		// Generator types really represent generator handles in LLVM
		class GenType : public Type {
		private:
			Type *outType;
			explicit GenType(Type *outType);
		public:
			GenType(GenType const&)=delete;
			void operator=(FuncType const&)=delete;

			llvm::Value *defaultValue(llvm::BasicBlock *block) override;
			llvm::Value *done(llvm::Value *self, llvm::BasicBlock *block);
			void resume(llvm::Value *self, llvm::BasicBlock *block);
			llvm::Value *promise(llvm::Value *self, llvm::BasicBlock *block);
			void destroy(llvm::Value *self, llvm::BasicBlock *block);

			bool is(Type *type) const override;
			unsigned numBaseTypes() const override;
			Type *getBaseType(unsigned idx) const override;
			llvm::Type *getLLVMType(llvm::LLVMContext &context) const override;
			seq_int_t size(llvm::Module *module) const override;

			GenType *asGen() override;

			static GenType *get(Type *outType) noexcept;
			static GenType *get() noexcept;

			GenType *clone(Generic *ref) override;
		};

		class PartialFuncType : public Type {
		private:
			Type *callee;
			std::vector<Type *> callTypes;
			RecordType *contents;

			// callTypes[i] == null means the argument is unknown
			PartialFuncType(Type *callee, std::vector<Type *> callTypes);
		public:
			PartialFuncType(PartialFuncType const&)=delete;
			void operator=(PartialFuncType const&)=delete;

			std::vector<Type *> getCallTypes() const;

			bool isAtomic() const override;

			llvm::Value *call(BaseFunc *base,
			                  llvm::Value *self,
			                  const std::vector<llvm::Value *>& args,
			                  llvm::BasicBlock *block) override;

			llvm::Value *defaultValue(llvm::BasicBlock *block) override;

			bool is(Type *type) const override;
			Type *getCallType(const std::vector<Type *>& inTypes) override;
			llvm::Type *getLLVMType(llvm::LLVMContext &context) const override;
			seq_int_t size(llvm::Module *module) const override;
			static PartialFuncType *get(Type *callee, std::vector<types::Type *> callTypes);

			llvm::Value *make(llvm::Value *func, std::vector<llvm::Value *> args, llvm::BasicBlock *block);
			PartialFuncType *clone(Generic *ref) override;
		};

	}
}

#endif /* SEQ_FUNCT_H */
