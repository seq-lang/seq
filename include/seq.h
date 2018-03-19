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
#include "io.h"
#include "exc.h"
#include "common.h"

namespace seq {

	struct PipelineAggregator {
		Seq *base;
		std::vector<Pipeline> pipelines;
		explicit PipelineAggregator(Seq *base);
		void add(Pipeline pipeline);
		Pipeline operator|(Pipeline to);
		Pipeline operator|(PipelineList to);
	};

	class Seq {
	private:
		std::string src;
		std::vector<Pipeline> pipelines;
		std::shared_ptr<std::map<SeqData, llvm::Value *>> outs;
		llvm::Function *func;
		llvm::BasicBlock *onceBlock;
		llvm::BasicBlock *preambleBlock;
		void codegen(llvm::Module *module, llvm::LLVMContext& context);

		friend PipelineAggregator;
	public:
		Seq();

		PipelineAggregator main;
		PipelineAggregator once;

		void source(std::string source);
		void execute(bool debug=false);
		void add(Pipeline pipeline);
		llvm::BasicBlock *getOnce() const;
		llvm::BasicBlock *getPreamble() const;

		Pipeline operator|(Pipeline to);
		Pipeline operator|(PipelineList to);
	};

	namespace types {
		static Type& Any   = *AnyType::get();
		static Type& Base  = *BaseType::get();
		static Type& Void  = *VoidType::get();
		static Type& Seq   = *SeqType::get();
		static Type& Int   = *IntType::get();
		static Type& Float = *FloatType::get();
	}

}

#endif /* SEQ_SEQ_H */
