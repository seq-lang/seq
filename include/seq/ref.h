#ifndef SEQ_REF_H
#define SEQ_REF_H

#include <vector>
#include <map>
#include <utility>
#include "funct.h"
#include "record.h"

namespace seq {
	class Func;

	namespace types {
		class GenericType;

		class RefType : public Type {
		private:
			RefType *root;
			RecordType *contents;
			std::map<std::string, Func *> methods;
			std::vector<GenericType *> generics;
			llvm::StructType *typeCached;
			std::map<void *, void *> cloneCache;
			std::vector<std::pair<std::vector<Type *>, RefType *>> realizationCache;
			mutable bool llvmTypeInProgress;
			explicit RefType(std::string name);
		public:
			RefType(RefType const&)=delete;
			void operator=(RefType const&)=delete;

			RecordType *getContents();
			void setContents(RecordType *contents);
			void addMethod(std::string name, Func *func);
			void addGenerics(unsigned count);
			void setGeneric(unsigned idx, Type *type);
			GenericType *getGeneric(unsigned idx);

			bool seenClone(void *p);
			void *getClone(void *p);
			void addClone(void *p, void *clone);

			RefType *realize(std::vector<Type *> types);

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

			RefType *clone(RefType *ref) override;
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

			MethodType *clone(RefType *ref) override;
		};

		class GenericType : public Type {
		private:
			RefType *ref;
			Type *type;
		public:
			explicit GenericType(RefType *ref);
			void realize(Type *type);
			void release();
			void ensure() const;
			Type *getType() const;

			std::string getName() const override;
			Type *getParent() const override;
			SeqData getKey() const override;
			VTable& getVTable() override;

			bool isAtomic() const override;

			std::string copyFuncName() override;
			std::string printFuncName() override;
			std::string allocFuncName() override;

			llvm::Value *loadFromAlloca(BaseFunc *base,
			                            llvm::Value *var,
			                            llvm::BasicBlock *block) override;

			llvm::Value *storeInAlloca(BaseFunc *base,
			                           llvm::Value *self,
			                           llvm::BasicBlock *block,
			                           bool storeDefault=false) override;

			llvm::Value *eq(BaseFunc *base,
			                llvm::Value *self,
			                llvm::Value *other,
			                llvm::BasicBlock *block) override;

			llvm::Value *copy(BaseFunc *base,
			                          llvm::Value *self,
			                          llvm::BasicBlock *block) override;

			void finalizeCopy(llvm::Module *module, llvm::ExecutionEngine *eng) override;

			void print(BaseFunc *base,
			           llvm::Value *self,
			           llvm::BasicBlock *block) override;

			void finalizePrint(llvm::Module *module, llvm::ExecutionEngine *eng) override;

			void serialize(BaseFunc *base,
			               llvm::Value *self,
			               llvm::Value *fp,
			               llvm::BasicBlock *block) override;

			void finalizeSerialize(llvm::Module *module, llvm::ExecutionEngine *eng) override;

			llvm::Value *deserialize(BaseFunc *base,
			                         llvm::Value *fp,
			                         llvm::BasicBlock *block) override;

			void finalizeDeserialize(llvm::Module *module, llvm::ExecutionEngine *eng) override;

			llvm::Value *alloc(llvm::Value *count, llvm::BasicBlock *block) override;
			llvm::Value *alloc(seq_int_t count, llvm::BasicBlock *block) override;

			void finalizeAlloc(llvm::Module *module, llvm::ExecutionEngine *eng) override;

			llvm::Value *load(BaseFunc *base,
			                  llvm::Value *ptr,
			                  llvm::Value *idx,
			                  llvm::BasicBlock *block) override;

			void store(BaseFunc *base,
			           llvm::Value *self,
			           llvm::Value *ptr,
			           llvm::Value *idx,
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

			llvm::Value *call(BaseFunc *base,
			                  llvm::Value *self,
			                  std::vector<llvm::Value *> args,
			                  llvm::BasicBlock *block) override;

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
			OpSpec findUOp(const std::string& symbol) override;
			OpSpec findBOp(const std::string& symbol, Type *rhsType) override;

			bool is(Type *type) const override;
			bool isGeneric(Type *type) const override;
			bool isChildOf(Type *type) const override;
			Type *getBaseType(seq_int_t idx) const override;
			Type *getCallType(std::vector<Type *> inTypes) override;
			Type *getConstructType(std::vector<Type *> inTypes) override;
			llvm::Type *getLLVMType(llvm::LLVMContext& context) const override;
			seq_int_t size(llvm::Module *module) const override;
			Mem& operator[](seq_int_t size) override;
			static GenericType *get(RefType *ref);

			GenericType *clone(RefType *ref) override;
		};

	}
}

#endif /* SEQ_REF_H */
