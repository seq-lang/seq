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
			bool isAbstract() const override;
			VTable& getVTable() override;

			bool isAtomic() const override;

			std::string allocFuncName() override;

			void print(llvm::Value *self, llvm::BasicBlock *block) override;

			void finalizePrint(llvm::Module *module, llvm::ExecutionEngine *eng) override;

			llvm::Value *alloc(llvm::Value *count, llvm::BasicBlock *block) override;
			llvm::Value *alloc(seq_int_t count, llvm::BasicBlock *block) override;

			llvm::Value *call(BaseFunc *base,
			                  llvm::Value *self,
			                  const std::vector<llvm::Value *>& args,
			                  llvm::BasicBlock *block) override;

			llvm::Value *memb(llvm::Value *self,
			                  const std::string& name,
			                  llvm::BasicBlock *block) override;

			Type *membType(const std::string& name) override;

			llvm::Value *setMemb(llvm::Value *self,
			                     const std::string& name,
			                     llvm::Value *val,
			                     llvm::BasicBlock *block) override;

			void addMethod(std::string name,
			               BaseFunc *func,
			               bool force) override;

			llvm::Value *staticMemb(const std::string& name, llvm::BasicBlock *block) override;

			Type *staticMembType(const std::string& name) override;

			llvm::Value *defaultValue(llvm::BasicBlock *block) override;

			llvm::Value *boolValue(llvm::Value *self, llvm::BasicBlock *block) override;

			void initOps() override;
			void initFields() override;
			Type *magicOut(const std::string& name, std::vector<Type *> args) override;
			llvm::Value *callMagic(const std::string& name,
			                       std::vector<Type *> argTypes,
			                       llvm::Value *self,
			                       std::vector<llvm::Value *> args,
			                       llvm::BasicBlock *block) override;

			bool is(Type *type) const override;
			bool isGeneric(Type *type) const override;
			unsigned numBaseTypes() const override;
			Type *getBaseType(unsigned idx) const override;
			Type *getCallType(const std::vector<Type *>& inTypes) override;
			llvm::Type *getLLVMType(llvm::LLVMContext& context) const override;
			seq_int_t size(llvm::Module *module) const override;

			RecordType *asRec() override;
			RefType *asRef() override;
			GenType *asGen() override;
			OptionalType *asOpt() override;

			static GenericType *get();

			GenericType *clone(Generic *ref) override;
			bool findInType(types::Type *type, std::vector<unsigned>& path);
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
		void addGenerics(int count);
		unsigned numGenerics() const;
		types::GenericType *getGeneric(int idx) const;
		bool seenClone(void *p);
		void *getClone(void *p);
		void addClone(void *p, void *clone);
		Generic *realizeGeneric(std::vector<types::Type *> types);
		std::vector<types::Type *> deduceTypesFromArgTypes(const std::vector<types::Type *>& inTypes,
		                                                   const std::vector<types::Type *>& argTypes);
	};

	template<typename T = types::Type>
	static bool typeMatch(const std::vector<T*>& v1, const std::vector<T*>& v2)
	{
		if (v1.size() != v2.size())
			return false;

		for (unsigned i = 0; i < v1.size(); i++) {
			if (!types::is(v1[i], v2[i]))
				return false;
		}

		return true;
	}
}

#endif /* SEQ_GENERIC_H */
