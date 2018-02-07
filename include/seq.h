#ifndef SEQ_SEQ_H
#define SEQ_SEQ_H

#include <cstdint>
#include <string>
#include <vector>
#include <map>

#include "llvm/ADT/APInt.h"
#include "llvm/IR/Verifier.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/ExecutionEngine/OrcMCJITReplacement.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"

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
		void codegen(llvm::Module *module, llvm::LLVMContext& context);
	public:
		Seq();
		void source(std::string source);
		void execute(bool debug=false);
		void add(Pipeline *pipeline);

		Pipeline& operator|(Pipeline& to);
		Pipeline& operator|(Stage& to);
	};
}

#endif /* SEQ_SEQ_H */
