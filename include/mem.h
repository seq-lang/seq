#ifndef SEQ_MEM_H
#define SEQ_MEM_H

#include <cstdint>
#include "stage.h"
#include "pipeline.h"
#include "var.h"
#include "types.h"

namespace seq {
	class Seq;
	class LoadStore;

	class Mem  {
	private:
		friend Seq;

		Mem(types::Type *of, uint32_t size, Seq *base);

		Seq *base;
		types::Type *of;
		unsigned size;
		llvm::Function *mallocFunc;
		llvm::GlobalVariable *ptr;

		void makeMalloc(llvm::Module *module, llvm::LLVMContext& context);
	public:
		types::Type *getType() const;
		Seq *getBase() const;

		void finalizeMalloc(llvm::ExecutionEngine *eng);
		void codegenAlloc(llvm::Module *module, llvm::LLVMContext& context);

		llvm::Value *codegenLoad(llvm::Module *module,
		                         llvm::LLVMContext& context,
		                         llvm::BasicBlock *block,
		                         llvm::Value *idx);

		void codegenStore(llvm::Module *module,
		                  llvm::LLVMContext& context,
		                  llvm::BasicBlock *block,
		                  llvm::Value *idx,
		                  llvm::Value *val);

		LoadStore& operator[](Var& idx);
	};

	class LoadStore : public Stage {
	private:
		Mem *mem;
		Var *idx;
		bool isStore;
	public:
		LoadStore(Mem *mem, Var *idx);

		void validate() override;
		void codegen(llvm::Module *module, llvm::LLVMContext& context) override;
		void finalize(llvm::ExecutionEngine *eng) override;

		Pipeline operator|(Pipeline to) override;
	};
}

#endif /* SEQ_MEM_H */
