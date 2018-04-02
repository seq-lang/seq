#ifndef SEQ_CHUNK_H
#define SEQ_CHUNK_H

#include "func.h"
#include "stage.h"

namespace seq {
	class Chunk : public Stage {
	private:
		Func& key;
	public:
		explicit Chunk(Func& func);
		void validate() override;
		void codegen(llvm::Module *module) override;
		static Chunk& make(Func& key);
	};
}

#endif /* SEQ_CHUNK_H */
