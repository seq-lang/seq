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

			virtual llvm::Type *getLLVMType(llvm::LLVMContext& context);

			virtual void callPrint(std::shared_ptr<std::map<SeqData, llvm::Value *>> outs, llvm::BasicBlock *block);

			virtual void finalizePrint(llvm::ExecutionEngine *eng);

			virtual void callSerialize(std::shared_ptr<std::map<SeqData, llvm::Value *>> outs,
			                           llvm::BasicBlock *block,
			                           std::string file);

			virtual void finalizeSerialize(llvm::ExecutionEngine *eng);

			virtual void callDeserialize(std::shared_ptr<std::map<SeqData, llvm::Value *>> outs,
			                             llvm::BasicBlock *block,
			                             std::string file);

			virtual void finalizeDeserialize(llvm::ExecutionEngine *eng);

			virtual void callSerializeArray(std::shared_ptr<std::map<SeqData, llvm::Value *>> outs,
			                                llvm::BasicBlock *block,
			                                std::string file);

			virtual void finalizeSerializeArray(llvm::ExecutionEngine *eng);

			virtual void callDeserializeArray(std::shared_ptr<std::map<SeqData, llvm::Value *>> outs,
			                                  llvm::BasicBlock *block,
			                                  std::string file);

			virtual void finalizeDeserializeArray(llvm::ExecutionEngine *eng);

			virtual llvm::Value *codegenLoad(llvm::BasicBlock *block,
			                                 llvm::Value *ptr,
			                                 llvm::Value *idx);

			virtual void codegenStore(llvm::BasicBlock *block,
			                          llvm::Value *ptr,
			                          llvm::Value *idx,
			                          llvm::Value *val);

			virtual bool isChildOf(Type *type);
			std::string getName() const;
			SeqData getKey() const;
			virtual seq_int_t size() const;
			Mem& operator[](seq_int_t size);
		};

	}
}

#endif /* SEQ_TYPES_H */
