#ifndef SEQ_SEQ_H
#define SEQ_SEQ_H

#include <cstdlib>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <initializer_list>

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
		Pipeline addWithIndex(Pipeline to, seq_int_t idx);
		Pipeline operator|(Pipeline to);
		Pipeline operator|(PipelineList to);
		Pipeline operator|(Var& to);
	};

	struct PipelineAggregatorProxy {
		PipelineAggregator& aggr;
		seq_int_t idx;
		PipelineAggregatorProxy(PipelineAggregator& aggr, seq_int_t idx);
		explicit PipelineAggregatorProxy(PipelineAggregator& aggr);
		Pipeline operator|(Pipeline to);
		Pipeline operator|(PipelineList to);
		Pipeline operator|(Var& to);
	};

	class Seq {
	private:
		llvm::LLVMContext context;
		std::vector<std::string> sources;
		std::vector<Pipeline> pipelines;
		std::array<ValMap, io::MAX_INPUTS> outs;
		llvm::Function *func;
		llvm::BasicBlock *preambleBlock;

		void codegen(llvm::Module *module);
		bool singleInput() const;

		friend PipelineAggregator;
	public:
		Seq();

		PipelineAggregator main;
		PipelineAggregator once;
		PipelineAggregator last;

		llvm::LLVMContext& getContext();
		void source(std::string s);

		template<typename ...T>
		void source(std::string s, T... etc)
		{
			source(std::move(s));
			source(etc...);
		}

		void execute(bool debug=false);
		void add(Pipeline pipeline);
		llvm::BasicBlock *getPreamble() const;

		Pipeline operator|(Pipeline to);
		Pipeline operator|(PipelineList to);
		Pipeline operator|(Var& to);

		PipelineAggregatorProxy operator[](unsigned idx);
	};

	namespace types {
		static AnyType&   Any   = *AnyType::get();
		static BaseType&  Base  = *BaseType::get();
		static VoidType&  Void  = *VoidType::get();
		static SeqType&   Seq   = *SeqType::get();
		static IntType&   Int   = *IntType::get();
		static FloatType& Float = *FloatType::get();
		static ArrayType& Array = *ArrayType::get();
	}

}

#endif /* SEQ_SEQ_H */
