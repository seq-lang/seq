#ifndef SEQ_GENERIC_H
#define SEQ_GENERIC_H

#include <cassert>
#include <vector>
#include <map>
#include "seq/types.h"

namespace seq {
	namespace types {
		class GenericType : public Type {
		private:
			bool aboutToBeRealized;
			std::string genericName;
			Type *type;
		public:
			GenericType();
			void markAboutToBeRealized();
			void unmarkAboutToBeRealized();
			void setName(std::string name);
			void realize(Type *type);
			bool realized() const;
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
			                           bool storeDefault) override;

			llvm::Value *storeInAlloca(BaseFunc *base,
			                           llvm::Value *self,
			                           llvm::BasicBlock *block) override;

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

			void addMethod(std::string name, BaseFunc *func) override;

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
			static GenericType *get();

			GenericType *clone(Generic *ref) override;
		};
	}

	class Generic {
	private:
		bool performCaching;
		Generic *root;

		std::vector<types::GenericType *> generics;
		std::vector<std::pair<std::vector<types::Type *>, Generic *>> realizationCache;
		std::map<void *, void *> cloneCache;
	public:
		explicit Generic(bool performCaching);

		virtual std::string genericName()=0;
		virtual Generic *clone(Generic *ref)=0;

		bool unrealized();
		std::vector<types::Type *> getRealizedTypes() const;
		bool is(Generic *other) const;
		Generic *findCachedRealizedType(std::vector<types::Type *> types) const;
		void setCloneBase(Generic *x, Generic *ref);
		void addGenerics(unsigned count);
		unsigned numGenerics();
		types::GenericType *getGeneric(unsigned idx);
		bool seenClone(void *p);
		void *getClone(void *p);
		void addClone(void *p, void *clone);
		Generic *realizeGeneric(std::vector<types::Type *> types);
		int findGenericParameter(types::GenericType *type);
	};

	template<typename T = types::Type>
	static bool typeMatch(const std::vector<T*>& v1, const std::vector<T*>& v2)
	{
		if (v1.size() != v2.size())
			return false;

		for (unsigned i = 0; i < v1.size(); i++) {
			if (!v1[i]->is(v2[i]) && !v2[i]->is(v1[i]))
				return false;
		}

		return true;
	}
}

#endif /* SEQ_GENERIC_H */
