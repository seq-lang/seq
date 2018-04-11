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
			void *serialize = nullptr;
			void *deserialize = nullptr;
			void *serializeArray = nullptr;
			void *deserializeArray = nullptr;
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
			virtual std::string serializeFuncName() { return "serialize" + getName(); }
			virtual std::string deserializeFuncName() { return "deserialize" + getName(); }
			virtual std::string serializeArrayFuncName() { return "serialize" + getName() + "Array"; }
			virtual std::string deserializeArrayFuncName() { return "deserialize" + getName() + "Array"; }
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
			                           llvm::BasicBlock *block,
			                           std::string file);

			virtual void finalizeSerialize(llvm::Module *module, llvm::ExecutionEngine *eng);

			virtual void callDeserialize(BaseFunc *base,
			                             ValMap outs,
			                             llvm::BasicBlock *block,
			                             std::string file);

			virtual void finalizeDeserialize(llvm::Module *module, llvm::ExecutionEngine *eng);

			virtual void callSerializeArray(BaseFunc *base,
			                                ValMap outs,
			                                llvm::BasicBlock *block,
			                                std::string file);

			virtual void finalizeSerializeArray(llvm::Module *module, llvm::ExecutionEngine *eng);

			virtual void callDeserializeArray(BaseFunc *base,
			                                  ValMap outs,
			                                  llvm::BasicBlock *block,
			                                  std::string file);

			virtual void finalizeDeserializeArray(llvm::Module *module, llvm::ExecutionEngine *eng);

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

			virtual bool isChildOf(Type *type);
			std::string getName() const;
			SeqData getKey() const;
			virtual llvm::Type *getLLVMType(llvm::LLVMContext& context);
			virtual seq_int_t size() const;
			Mem& operator[](seq_int_t size);
		};

	}
}

#endif /* SEQ_TYPES_H */
