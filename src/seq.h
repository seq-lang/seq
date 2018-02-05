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
		bool added;
	public:
		friend Stage;
		Pipeline(Stage *head, Stage *tail);
		Pipeline();
		Pipeline& operator|(Stage& to);
		Pipeline& operator|(Pipeline& to);
		Stage *getHead() const;
		Stage *getTail() const;
		bool isLinked() const;
		bool isAdded() const;
		void setAdded();
		void validate();
	};

	class Seq;

	class Var {
	protected:
		bool assigned;
		types::Type type;
		Pipeline *pipeline;
		Seq *base;
	public:
		Var();
		explicit Var(types::Type type);
		Pipeline& operator|(Pipeline& to);
		Pipeline& operator|(Stage& to);

		Var& operator=(Pipeline& to);
		Var& operator=(Stage& to);
	};

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
		llvm::Function *getFunc() const;

		Pipeline& operator|(Pipeline& to);
		Pipeline& operator|(Stage& to);
	};

	namespace stageutil {
		Copy& copy();
		Filter& filter(std::string name, SeqPred op);
		Op& op(std::string name, SeqOp op);
		Print& print();
		RevComp& revcomp();
		Split& split(uint32_t k, uint32_t step);
		Substr& substr(uint32_t start, uint32_t len);
	}
}

std::ostream& operator<<(std::ostream& os, seq::Stage& stage);
std::ostream& operator<<(std::ostream& os, seq::Pipeline& stage);

#endif /* SEQ_SEQ_H */
