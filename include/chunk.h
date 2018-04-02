#ifndef SEQ_CHUNK_H
#define SEQ_CHUNK_H

#include "func.h"
#include "stage.h"

namespace seq {
	class Chunk : public Stage {
	private:
		Func *key;
	public:
		explicit Chunk(Func *key);
		Chunk();
		void validate() override;
		void codegen(llvm::Module *module) override;
		static Chunk& make(Func& key);
		static Chunk& make();
	};
}

#endif /* SEQ_CHUNK_H */
