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

#include "stages/stage.h"
#include "stages/copy.h"
#include "stages/filter.h"
#include "stages/op.h"
#include "stages/print.h"
#include "stages/revcomp.h"
#include "stages/split.h"
#include "stages/substr.h"
#include "io.h"

namespace seq {
	class Pipeline {
	private:
		Stage *head;
		Stage *tail;
		bool linked;
	public:
		friend Stage;
		Pipeline(Stage *head, Stage *tail);
		Pipeline& operator|(Stage& to);
		Pipeline& operator|(Pipeline& to);
		Stage *getHead();
		void validate();
	};

	class Seq : public Stage {
	private:
		std::string src;
		llvm::Function *func;
	public:
		Seq();

		void codegen(llvm::Module *module, llvm::LLVMContext& context) override;
		void source(std::string source);
		void execute(bool debug=false);
	};
}

std::ostream& operator<<(std::ostream& os, seq::Stage& stage);
std::ostream& operator<<(std::ostream& os, seq::Pipeline& stage);

#endif /* SEQ_SEQ_H */
