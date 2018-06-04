#ifndef SEQ_TYPES_H
#define SEQ_TYPES_H

#include <string>
#include <map>
#include <functional>
#include <utility>
#include "llvm.h"
#include "seqdata.h"
#include "ops.h"
#include "util.h"

namespace seq {

	class BaseFunc;
	class Mem;

	struct OpSpec;

	namespace types {

		class Type;

		struct VTable {
			void *copy = nullptr;
			void *print = nullptr;
			std::map<std::string, std::pair<int, Type *>> fields;
			std::vector<OpSpec> ops;
		};

		class Type {
		protected:
			std::string name;
			Type *parent;
			SeqData key;
			VTable vtable;
		public:
			Type(std::string name, Type *parent, SeqData key);
			Type(std::string name, Type *parent);

			virtual std::string copyFuncName() { return "copy" + getName(); }
			virtual std::string printFuncName() { return "print" + getName(); }
			virtual std::string allocFuncName() { return "malloc"; }

			virtual llvm::Type *getFuncType(llvm::LLVMContext& context, Type *outType);

			virtual llvm::Function *makeFuncOf(llvm::Module *module, Type *outType);

			virtual llvm::Value *setFuncArgs(llvm::Function *func,
			                                 llvm::BasicBlock *block);

			virtual llvm::Value *callFuncOf(llvm::Value *func,
					                        llvm::Value *arg,
			                                llvm::BasicBlock *block);

			virtual llvm::Value *loadFromAlloca(BaseFunc *base,
			                                    llvm::Value *var,
			                                    llvm::BasicBlock *block);

			virtual llvm::Value *storeInAlloca(BaseFunc *base,
			                                   llvm::Value *self,
			                                   llvm::BasicBlock *block,
			                                   bool storeDefault=false);

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

			virtual llvm::Value *alloc(BaseFunc *base,
			                           llvm::Value *count,
			                           llvm::BasicBlock *block);

			virtual llvm::Value *alloc(BaseFunc *base,
			                           seq_int_t count,
			                           llvm::BasicBlock *block);

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

			virtual llvm::Value *call(BaseFunc *base,
			                          llvm::Value *self,
			                          llvm::Value *arg,
			                          llvm::BasicBlock *block);

			virtual llvm::Value *memb(llvm::Value *self,
			                          const std::string& name,
			                          llvm::BasicBlock *block);

			virtual Type *membType(const std::string& name);

			virtual llvm::Value *setMemb(llvm::Value *self,
			                             const std::string& name,
			                             llvm::Value *val,
			                             llvm::BasicBlock *block);

			virtual llvm::Value *defaultValue(llvm::BasicBlock *block);

			virtual void initOps();
			virtual void initFields();
			virtual OpSpec findUOp(const std::string& symbol);
			virtual OpSpec findBOp(const std::string& symbol, Type *rhsType);

			virtual bool is(Type *type) const;
			virtual bool isGeneric(Type *type) const;
			virtual bool isChildOf(Type *type) const;
			std::string getName() const;
			SeqData getKey() const;
			virtual Type *getBaseType(seq_int_t idx) const;
			virtual Type *getCallType(Type *inType);
			virtual llvm::Type *getLLVMType(llvm::LLVMContext& context) const;
			virtual seq_int_t size(llvm::Module *module) const;
			Mem& operator[](seq_int_t size);
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
