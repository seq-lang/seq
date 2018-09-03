#ifndef SEQ_TYPES_H
#define SEQ_TYPES_H

#include <string>
#include <map>
#include <functional>
#include <utility>
#include "llvm.h"
#include "ops.h"
#include "lib.h"

#define SEQ_STRINGIFY(x) #x
#define SEQ_TOSTRING(x)  SEQ_STRINGIFY(x)

#define SEQ_ASSIGN_VTABLE_FIELD(field, value) \
    do { \
	    vtable.field = (void *)(value); \
        vtable.field##Name = SEQ_TOSTRING(value); \
    } while (0)

namespace seq {

	class BaseFunc;
	class Func;
	class Generic;

	struct OpSpec;

	namespace types {

		class Type;

		struct VTable {
			void *copy = nullptr;
			void *print = nullptr;

			std::string copyName = "";
			std::string printName = "";

			std::map<std::string, std::pair<int, Type *>> fields = {};
			std::map<std::string, BaseFunc *> methods = {};
			std::vector<OpSpec> ops = {};
		};

		enum Key {
			NONE,
			SEQ,
			INT,
			FLOAT,
			BOOL,
			STR,
			ARRAY,
			RECORD,
			FUNC,
			REF,
			OPTIONAL,
		};

		class Type {
		protected:
			std::string name;
			Type *parent;
			Key key;
			VTable vtable;
		public:
			Type(std::string name, Type *parent, Key key);
			Type(std::string name, Type *parent);

			virtual std::string getName() const;
			virtual Type *getParent() const;
			virtual Key getKey() const;
			virtual VTable& getVTable();

			virtual bool isAtomic() const;

			virtual std::string allocFuncName() { return isAtomic() ? "seq_alloc_atomic" : "seq_alloc"; }

			virtual llvm::Value *loadFromAlloca(BaseFunc *base,
			                                    llvm::Value *var,
			                                    llvm::BasicBlock *block);

			virtual llvm::Value *storeInAlloca(BaseFunc *base,
			                                   llvm::Value *self,
			                                   llvm::BasicBlock *block,
			                                   bool storeDefault);

			virtual llvm::Value *storeInAlloca(BaseFunc *base,
			                                   llvm::Value *self,
			                                   llvm::BasicBlock *block);

			virtual llvm::Value *eq(BaseFunc *base,
			                        llvm::Value *self,
			                        llvm::Value *other,
			                        llvm::BasicBlock *block);

			virtual llvm::Value *copy(BaseFunc *base,
			                          llvm::Value *self,
			                          llvm::BasicBlock *block);

			virtual void finalizeCopy(llvm::Module *module, llvm::ExecutionEngine *eng);

			virtual void print(BaseFunc *base,
			                   llvm::Value *self,
			                   llvm::BasicBlock *block);

			virtual void finalizePrint(llvm::Module *module, llvm::ExecutionEngine *eng);

			virtual void serialize(BaseFunc *base,
			                       llvm::Value *self,
			                       llvm::Value *fp,
			                       llvm::BasicBlock *block);

			virtual void finalizeSerialize(llvm::Module *module, llvm::ExecutionEngine *eng);

			virtual llvm::Value *deserialize(BaseFunc *base,
			                                 llvm::Value *fp,
			                                 llvm::BasicBlock *block);

			virtual void finalizeDeserialize(llvm::Module *module, llvm::ExecutionEngine *eng);

			virtual llvm::Value *alloc(llvm::Value *count, llvm::BasicBlock *block);
			virtual llvm::Value *alloc(seq_int_t count, llvm::BasicBlock *block);

			virtual void finalizeAlloc(llvm::Module *module, llvm::ExecutionEngine *eng);

			virtual llvm::Value *load(BaseFunc *base,
			                          llvm::Value *ptr,
			                          llvm::Value *idx,
			                          llvm::BasicBlock *block);

			virtual void store(BaseFunc *base,
			                   llvm::Value *self,
			                   llvm::Value *ptr,
			                   llvm::Value *idx,
			                   llvm::BasicBlock *block);

			virtual llvm::Value *indexLoad(BaseFunc *base,
			                               llvm::Value *self,
			                               llvm::Value *idx,
			                               llvm::BasicBlock *block);

			virtual void indexStore(BaseFunc *base,
			                        llvm::Value *self,
			                        llvm::Value *idx,
			                        llvm::Value *val,
			                        llvm::BasicBlock *block);

			virtual llvm::Value *indexSlice(BaseFunc *base,
			                                llvm::Value *self,
			                                llvm::Value *from,
			                                llvm::Value *to,
			                                llvm::BasicBlock *block);

			virtual llvm::Value *indexSliceNoFrom(BaseFunc *base,
			                                      llvm::Value *self,
			                                      llvm::Value *to,
			                                      llvm::BasicBlock *block);

			virtual llvm::Value *indexSliceNoTo(BaseFunc *base,
			                                    llvm::Value *self,
			                                    llvm::Value *from,
			                                    llvm::BasicBlock *block);

			virtual Type *indexType() const;

			virtual llvm::Value *call(BaseFunc *base,
			                          llvm::Value *self,
			                          std::vector<llvm::Value *> args,
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

			virtual void addMethod(std::string name, BaseFunc *func);

			virtual BaseFunc *getMethod(const std::string& name);

			virtual llvm::Value *staticMemb(const std::string& name, llvm::BasicBlock *block);

			virtual Type *staticMembType(const std::string& name);

			virtual llvm::Value *defaultValue(llvm::BasicBlock *block);

			virtual llvm::Value *construct(BaseFunc *base,
			                               std::vector<llvm::Value *> args,
			                               llvm::BasicBlock *block);

			virtual void initOps();
			virtual void initFields();
			virtual OpSpec findUOp(const std::string& symbol);
			virtual OpSpec findBOp(const std::string& symbol, Type *rhsType);

			virtual bool is(Type *type) const;
			virtual bool isGeneric(Type *type) const;
			virtual bool isChildOf(Type *type) const;
			virtual Type *getBaseType(seq_int_t idx) const;
			virtual Type *getCallType(std::vector<Type *> inTypes);
			virtual Type *getConstructType(std::vector<Type *> inTypes);
			virtual llvm::Type *getLLVMType(llvm::LLVMContext& context) const;
			virtual seq_int_t size(llvm::Module *module) const;

			virtual Type *clone(Generic *ref);
		};

	}

	struct OpSpec {
		Op op;
		types::Type *rhsType;
		types::Type *outType;
		std::function<llvm::Value *(llvm::Value *, llvm::Value *, llvm::IRBuilder<>&)> codegen;
	};

}

#endif /* SEQ_TYPES_H */
