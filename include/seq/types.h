#ifndef SEQ_TYPES_H
#define SEQ_TYPES_H

#include <string>
#include <map>
#include <functional>
#include <utility>
#include "llvm.h"
#include "lib.h"

namespace seq {

	class BaseFunc;
	class Func;
	class Generic;

	struct MagicMethod;
	struct MagicOverload;

	namespace types {

		class Type;
		class RecordType;
		class RefType;
		class GenType;
		class OptionalType;

		struct VTable {
			std::map<std::string, std::pair<int, Type *>> fields = {};
			std::map<std::string, BaseFunc *> methods = {};
			std::vector<MagicMethod> magic = {};
			std::vector<MagicOverload> overloads = {};
		};

		class Type {
		protected:
			std::string name;
			Type *parent;
			bool abstract;
			VTable vtable;
			bool resolving;
		public:
			Type(std::string name, Type *parent, bool abstract=false);

			virtual std::string getName() const;
			virtual Type *getParent() const;
			virtual bool isAbstract() const;
			virtual VTable& getVTable();
			virtual bool isAtomic() const;

			virtual llvm::Value *alloc(llvm::Value *count, llvm::BasicBlock *block);

			virtual llvm::Value *call(BaseFunc *base,
			                          llvm::Value *self,
			                          const std::vector<llvm::Value *>& args,
			                          llvm::BasicBlock *block);

			virtual llvm::Value *memb(llvm::Value *self,
			                          const std::string& name,
			                          llvm::BasicBlock *block);

			virtual Type *membType(const std::string& name);

			virtual llvm::Value *setMemb(llvm::Value *self,
			                             const std::string& name,
			                             llvm::Value *val,
			                             llvm::BasicBlock *block);

			virtual bool hasMethod(const std::string& name);

			virtual void addMethod(std::string name,
			                       BaseFunc *func,
			                       bool force);

			virtual BaseFunc *getMethod(const std::string& name);

			virtual llvm::Value *staticMemb(const std::string& name, llvm::BasicBlock *block);

			virtual Type *staticMembType(const std::string& name);

			virtual llvm::Value *defaultValue(llvm::BasicBlock *block);

			virtual llvm::Value *boolValue(llvm::Value *self, llvm::BasicBlock *block);

			virtual void initOps();
			virtual void initFields();
			virtual Type *magicOut(const std::string& name, std::vector<Type *> args);
			virtual llvm::Value *callMagic(const std::string& name,
			                               std::vector<Type *> argTypes,
			                               llvm::Value *self,
			                               std::vector<llvm::Value *> args,
			                               llvm::BasicBlock *block);
			virtual void resolveTypes();

			virtual bool is(Type *type) const;
			virtual bool isGeneric(Type *type) const;
			virtual unsigned numBaseTypes() const;
			virtual Type *getBaseType(unsigned idx) const;
			virtual Type *getCallType(const std::vector<Type *>& inTypes);
			virtual llvm::Type *getLLVMType(llvm::LLVMContext& context) const;
			virtual size_t size(llvm::Module *module) const;

			/*
			 * The following method are basically for overriding
			 * dynamic_cast so that generic types can be converted
			 * to the types they represent easily.
			 */
			virtual RecordType *asRec();
			virtual RefType *asRef();
			virtual GenType *asGen();
			virtual OptionalType *asOpt();

			virtual Type *clone(Generic *ref);
		};

		bool is(Type *type1, Type *type2);
	}

	struct MagicMethod {
		std::string name;
		std::vector<types::Type *> args;
		types::Type *out;
		std::function<llvm::Value *(llvm::Value *,
		                            std::vector<llvm::Value *>,
		                            llvm::IRBuilder<>&)> codegen;

		BaseFunc *asFunc(types::Type *type) const;
	};

	struct MagicOverload {
		std::string name;
		BaseFunc *func;
	};

}

#define SEQ_MAGIC(self, args, builder) \
    [](llvm::Value *(self), std::vector<llvm::Value *> (args), llvm::IRBuilder<>& (builder))

#define SEQ_MAGIC_CAPT(self, args, builder) \
    [this](llvm::Value *(self), std::vector<llvm::Value *> (args), llvm::IRBuilder<>& (builder))


#endif /* SEQ_TYPES_H */
