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
#include "lambda.h"
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
		Pipeline operator|(Var& to);
	};

	class Seq {
	private:
		std::string src;
		std::vector<Pipeline> pipelines;
		std::shared_ptr<std::map<SeqData, llvm::Value *>> outs;
		llvm::Function *func;
		llvm::BasicBlock *preambleBlock;
		void codegen(llvm::Module *module);

		friend PipelineAggregator;
	public:
		Seq();

		PipelineAggregator main;
		PipelineAggregator once;
		PipelineAggregator last;

		void source(std::string source);
		void execute(bool debug=false);
		void add(Pipeline pipeline);
		llvm::BasicBlock *getPreamble() const;

		Pipeline operator|(Pipeline to);
		Pipeline operator|(PipelineList to);
	};

	namespace types {
		static AnyType&   Any   = *AnyType::get();
		static BaseType&  Base  = *BaseType::get();
		static VoidType&  Void  = *VoidType::get();
		static SeqType&   Seq   = *SeqType::get();
		static IntType&   Int   = *IntType::get();
		static FloatType& Float = *FloatType::get();
		static ArrayType& Array = *ArrayType::get(AnyType::get(), 0);
	}

}

#endif /* SEQ_SEQ_H */
