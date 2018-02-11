#ifndef SEQ_SEQ_H
#define SEQ_SEQ_H

#include <cstdint>
#include <string>
#include <vector>
#include <map>

#include "llvm.h"
#include "pipeline.h"
#include "stageutil.h"
#include "var.h"
#include "io.h"
#include "exc.h"

namespace seq {
	class Seq {
	private:
		std::string src;
		std::vector<Pipeline *> pipelines;
		llvm::Function *func;
		llvm::BasicBlock *preamble;
		void codegen(llvm::Module *module, llvm::LLVMContext& context);
	public:
		Seq();
		void source(std::string source);
		void execute(bool debug=false);
		void add(Pipeline *pipeline);
		llvm::BasicBlock *getPreamble() const;

		Pipeline& operator|(Pipeline& to);
		Pipeline& operator|(Stage& to);
	};
}

#endif /* SEQ_SEQ_H */
