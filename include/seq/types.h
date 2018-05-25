#ifndef SEQ_TYPES_H
#define SEQ_TYPES_H

#include <string>
#include <map>
#include <functional>
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

			virtual void setFuncArgs(llvm::Function *func,
			                         ValMap outs,
			                         llvm::BasicBlock *block);

			virtual llvm::Value *callFuncOf(llvm::Value *func,
					                        ValMap outs,
			                                llvm::BasicBlock *block);

			virtual llvm::Value *pack(BaseFunc *base,
			                          ValMap outs,
			                          llvm::BasicBlock *block);

			virtual void unpack(BaseFunc *base,
			                    llvm::Value *value,
			                    ValMap outs,
			                    llvm::BasicBlock *block);

			virtual llvm::Value *checkEq(BaseFunc *base,
			                             ValMap ins1,
			                             ValMap ins2,
			                             llvm::BasicBlock *block);

			virtual void callCopy(BaseFunc *base,
			                      ValMap ins,
			                      ValMap outs,
			                      llvm::BasicBlock *block);

			virtual void finalizeCopy(llvm::Module *module, llvm::ExecutionEngine *eng);

			virtual void callPrint(BaseFunc *base,
			                       ValMap outs,
			                       llvm::BasicBlock *block);

			virtual void finalizePrint(llvm::Module *module, llvm::ExecutionEngine *eng);

			virtual void callSerialize(BaseFunc *base,
			                           ValMap outs,
			                           llvm::Value *fp,
			                           llvm::BasicBlock *block);

			virtual void finalizeSerialize(llvm::Module *module, llvm::ExecutionEngine *eng);

			virtual void callDeserialize(BaseFunc *base,
			                             ValMap outs,
			                             llvm::Value *fp,
			                             llvm::BasicBlock *block);

			virtual void finalizeDeserialize(llvm::Module *module, llvm::ExecutionEngine *eng);

			virtual llvm::Value *codegenAlloc(BaseFunc *base,
			                                  llvm::Value *count,
			                                  llvm::BasicBlock *block);

			virtual llvm::Value *codegenAlloc(BaseFunc *base,
			                                  seq_int_t count,
			                                  llvm::BasicBlock *block);

			virtual void finalizeAlloc(llvm::Module *module, llvm::ExecutionEngine *eng);

			virtual void codegenLoad(BaseFunc *base,
			                         ValMap outs,
			                         llvm::BasicBlock *block,
			                         llvm::Value *ptr,
			                         llvm::Value *idx);

			virtual void codegenStore(BaseFunc *base,
			                          ValMap outs,
			                          llvm::BasicBlock *block,
			                          llvm::Value *ptr,
			                          llvm::Value *idx);

			virtual void codegenIndexLoad(BaseFunc *base,
			                              ValMap outs,
			                              llvm::BasicBlock *block,
			                              llvm::Value *ptr,
			                              llvm::Value *idx);

			virtual void codegenIndexStore(BaseFunc *base,
			                               ValMap outs,
			                               llvm::BasicBlock *block,
			                               llvm::Value *ptr,
			                               llvm::Value *idx);

			virtual void call(BaseFunc *base,
			                  ValMap ins,
			                  ValMap outs,
			                  llvm::Value *fn,
			                  llvm::BasicBlock *block);

			virtual void initOps();
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
