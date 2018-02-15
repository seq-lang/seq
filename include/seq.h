#ifndef SEQ_SEQ_H
#define SEQ_SEQ_H

#include <cstdlib>
#include <cstdint>
#include <string>
#include <vector>
#include <map>

#include "llvm.h"
#include "pipeline.h"
#include "stageutil.h"
#include "var.h"
#include "mem.h"
#include "types.h"
#include "io.h"
#include "exc.h"

namespace seq {
	class Seq {
	private:
		std::string src;
		std::vector<Pipeline *> pipelines;
		std::shared_ptr<std::map<SeqData, llvm::Value *>> outs;
		llvm::Function *func;
		llvm::BasicBlock *once;
		llvm::BasicBlock *preamble;
		void codegen(llvm::Module *module, llvm::LLVMContext& context);
	public:
		Seq();
		void source(std::string source);
		void execute(bool debug=false);
		void add(Pipeline *pipeline);
		llvm::BasicBlock *getOnce() const;
		llvm::BasicBlock *getPreamble() const;

		template<typename TYPE>
		Mem mem(uint32_t size)
		{
			return {TYPE::get(), size, this};
		}

		Pipeline& operator|(Pipeline& to);
		Pipeline& operator|(Stage& to);
	};
}

#endif /* SEQ_SEQ_H */
