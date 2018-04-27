#ifndef SEQ_TYPES_H
#define SEQ_TYPES_H

#include <string>
#include <map>
#include "llvm.h"
#include "seqdata.h"
#include "exc.h"
#include "util.h"

namespace seq {

	class BaseFunc;
	class Mem;

	namespace types {

		struct VTable {
			void *copy = nullptr;
			void *print = nullptr;
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

			virtual llvm::Function *makeFuncOf(llvm::Module *module, Type *outType);

			virtual void setFuncArgs(llvm::Function *func,
			                         ValMap outs,
			                         llvm::BasicBlock *block);

			virtual llvm::Value *callFuncOf(llvm::Function *func,
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

			virtual bool is(Type *type) const;
			virtual bool isGeneric(Type *type) const;
			virtual bool isChildOf(Type *type) const;
			std::string getName() const;
			SeqData getKey() const;
			virtual Type *getBaseType(seq_int_t idx) const;
			virtual llvm::Type *getLLVMType(llvm::LLVMContext& context) const;
			virtual seq_int_t size(llvm::Module *module) const;
			Mem& operator[](seq_int_t size);
		};

	}
}

#endif /* SEQ_TYPES_H */
