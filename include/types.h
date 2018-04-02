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
			void *print = nullptr;
			void *serialize = nullptr;
			void *deserialize = nullptr;
			void *serializeArray = nullptr;
			void *deserializeArray = nullptr;

			llvm::Function *allocFunc = nullptr;
			llvm::Function *printFunc = nullptr;
			llvm::Function *serializeFunc = nullptr;
			llvm::Function *deserializeFunc = nullptr;
			llvm::Function *serializeArrayFunc = nullptr;
			llvm::Function *deserializeArrayFunc = nullptr;
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

			virtual llvm::Function *makeFuncOf(llvm::Module *module,
			                                   ValMap outs,
			                                   Type *outType);

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

			virtual void callPrint(BaseFunc *base,
			                       ValMap outs,
			                       llvm::BasicBlock *block);

			virtual void finalizePrint(llvm::ExecutionEngine *eng);

			virtual void callSerialize(BaseFunc *base,
			                           ValMap outs,
			                           llvm::BasicBlock *block,
			                           std::string file);

			virtual void finalizeSerialize(llvm::ExecutionEngine *eng);

			virtual void callDeserialize(BaseFunc *base,
			                             ValMap outs,
			                             llvm::BasicBlock *block,
			                             std::string file);

			virtual void finalizeDeserialize(llvm::ExecutionEngine *eng);

			virtual void callSerializeArray(BaseFunc *base,
			                                ValMap outs,
			                                llvm::BasicBlock *block,
			                                std::string file);

			virtual void finalizeSerializeArray(llvm::ExecutionEngine *eng);

			virtual void callDeserializeArray(BaseFunc *base,
			                                  ValMap outs,
			                                  llvm::BasicBlock *block,
			                                  std::string file);

			virtual void finalizeDeserializeArray(llvm::ExecutionEngine *eng);

			virtual llvm::Value *codegenAlloc(BaseFunc *base,
			                                  seq_int_t count,
			                                  llvm::BasicBlock *block);

			virtual void finalizeAlloc(llvm::ExecutionEngine *eng);

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
