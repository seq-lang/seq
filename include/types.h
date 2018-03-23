#ifndef SEQ_TYPES_H
#define SEQ_TYPES_H

#include <string>
#include <map>
#include "llvm.h"
#include "seqdata.h"
#include "exc.h"
#include "util.h"

namespace seq {

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

			virtual void callPrint(ValMap outs, llvm::BasicBlock *block);

			virtual void finalizePrint(llvm::ExecutionEngine *eng);

			virtual void callSerialize(ValMap outs,
			                           llvm::BasicBlock *block,
			                           std::string file);

			virtual void finalizeSerialize(llvm::ExecutionEngine *eng);

			virtual void callDeserialize(ValMap outs,
			                             llvm::BasicBlock *block,
			                             std::string file);

			virtual void finalizeDeserialize(llvm::ExecutionEngine *eng);

			virtual void callSerializeArray(ValMap outs,
			                                llvm::BasicBlock *block,
			                                std::string file);

			virtual void finalizeSerializeArray(llvm::ExecutionEngine *eng);

			virtual void callDeserializeArray(ValMap outs,
			                                  llvm::BasicBlock *block,
			                                  std::string file);

			virtual void finalizeDeserializeArray(llvm::ExecutionEngine *eng);


			virtual void callAlloc(ValMap outs,
			                       seq_int_t count,
			                       llvm::BasicBlock *block);

			virtual void finalizeAlloc(llvm::ExecutionEngine *eng);

			virtual void codegenLoad(ValMap outs,
			                         llvm::BasicBlock *block,
			                         llvm::Value *ptr,
			                         llvm::Value *idx);

			virtual void codegenStore(ValMap outs,
			                          llvm::BasicBlock *block,
			                          llvm::Value *ptr,
			                          llvm::Value *idx);

			virtual bool isChildOf(Type *type);
			std::string getName() const;
			SeqData getKey() const;
			virtual llvm::Type *getLLVMType(llvm::LLVMContext& context);
			virtual llvm::Type *getLLVMArrayType(llvm::LLVMContext& context);
			virtual seq_int_t size() const;
			virtual seq_int_t arraySize() const;
			Mem& operator[](seq_int_t size);
		};

	}
}

#endif /* SEQ_TYPES_H */
