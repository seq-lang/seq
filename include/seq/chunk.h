#ifndef SEQ_CHUNK_H
#define SEQ_CHUNK_H

#include "func.h"
#include "expr.h"
#include "stage.h"

namespace seq {
	class Chunk : public Stage {
	private:
		Expr *key;
	public:
		explicit Chunk(Expr *key);
		explicit Chunk(Func *key);
		Chunk();
		void validate() override;
		void codegen(llvm::Module *module) override;
		static Chunk& make(Expr *key);
		static Chunk& make(Func& key);
		static Chunk& make();

		Chunk *clone(types::RefType *ref) override;
	};
}

#endif /* SEQ_CHUNK_H */
