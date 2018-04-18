#ifndef SEQ_FUNC_H
#define SEQ_FUNC_H

#include "stage.h"
#include "types.h"
#include "pipeline.h"
#include "var.h"
#include "common.h"

namespace seq {

	class Call;
	class MultiCall;

	struct CompilationContext {
		bool inOnce = false;
		bool inMain = false;
		bool inLast = false;
		bool inFunc = false;

		inline void reset()
		{
			inOnce = inMain = inLast = false;
		}
	};

	class BaseFunc {
	public:
		CompilationContext compilationContext;
	protected:
		llvm::Module *module;
		llvm::BasicBlock *initBlock;
		llvm::BasicBlock *preambleBlock;
		llvm::Function *initFunc;
		llvm::Function *func;
		BaseFunc();
	public:
		virtual void codegenInit(llvm::Module *module);
		virtual void finalizeInit(llvm::Module *module);
		virtual void codegen(llvm::Module *module)=0;
		virtual void codegenCall(BaseFunc *base,
		                         ValMap ins,
		                         ValMap outs,
		                         llvm::BasicBlock *block)=0;
		virtual void add(Pipeline pipeline)=0;

		llvm::LLVMContext& getContext();
		llvm::BasicBlock *getInit() const;
		llvm::BasicBlock *getPreamble() const;
		virtual types::Type *getInType() const;
		virtual types::Type *getOutType() const;
	};

	class Func : public BaseFunc {
	private:
		types::Type *inType;
		types::Type *outType;
		std::vector<Pipeline> pipelines;
		ValMap outs;

		/* for native functions */
		std::string name;
		void *rawFunc;
	public:
		Func(types::Type& inType,
		     types::Type& outType,
		     std::string name,
		     void *rawFunc);
		Func(types::Type& inType, types::Type& outType);
		void codegen(llvm::Module *module) override;
		llvm::Value *codegenCallRaw(BaseFunc *base, ValMap ins, llvm::BasicBlock *block);
		void codegenCall(BaseFunc *base,
		                 ValMap ins,
		                 ValMap outs,
		                 llvm::BasicBlock *block) override;
		void add(Pipeline pipeline) override;
		void finalize(llvm::Module *module, llvm::ExecutionEngine *eng);

		types::Type *getInType() const override;
		types::Type *getOutType() const override;

		Pipeline operator|(Pipeline to);
		Pipeline operator|(PipelineList& to);
		Pipeline operator|(Var& to);
		Pipeline operator<<(PipelineList& to);

		Call& operator()();
	};

	class FuncList {
		struct Node {
			Func& f;
			Node *next;

			explicit Node(Func& f);
		};

	public:
		Node *head;
		Node *tail;

		explicit FuncList(Func& f);
		FuncList& operator,(Func& f);
		MultiCall& operator()();
	};

	FuncList& operator,(Func& f1, Func& f2);

}

#endif /* SEQ_FUNC_H */
