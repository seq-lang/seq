#ifndef SEQ_MEM_H
#define SEQ_MEM_H

#include <cstdint>
#include "stage.h"
#include "pipeline.h"
#include "var.h"
#include "array.h"
#include "types.h"

namespace seq {
	class Seq;
	class LoadStore;

	class Mem : public Stage {
	public:
		Mem(types::Type *type, seq_int_t count);
		void codegen(llvm::Module *module) override;
		void finalize(llvm::ExecutionEngine *eng) override;
		static Mem& make(types::Type *type, seq_int_t count);
	};

	class LoadStore : public Stage {
	private:
		Var *ptr;
		Var *idx;
		bool isStore;
	public:
		LoadStore(Var *ptr, Var *idx);
		void validate() override;
		void codegen(llvm::Module *module) override;

		Pipeline operator|(Pipeline to) override;

		static LoadStore& make(Var *ptr, Var *idx);
	};
}

#endif /* SEQ_MEM_H */
