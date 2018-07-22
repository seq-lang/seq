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
	class Expr;

	class BaseFunc {
	protected:
		llvm::Module *module;
		llvm::BasicBlock *preambleBlock;
		llvm::Function *func;
		BaseFunc();
	public:
		virtual void codegen(llvm::Module *module)=0;
		virtual llvm::Value *codegenCall(BaseFunc *base,
		                                 std::vector<llvm::Value *> args,
		                                 llvm::BasicBlock *block)=0;
		virtual void codegenReturn(llvm::Value *val,
		                           types::Type *type,
		                           llvm::BasicBlock*& block)=0;
		virtual void codegenYield(llvm::Value *val,
		                          types::Type *type,
		                          llvm::BasicBlock*& block)=0;
		virtual void add(Pipeline pipeline)=0;

		llvm::LLVMContext& getContext();
		llvm::BasicBlock *getPreamble() const;
		virtual types::Type *getInType() const;
		virtual types::Type *getOutType() const;
		virtual llvm::Function *getFunc();

		virtual BaseFunc *clone(types::RefType *ref);
	};

	class Func : public BaseFunc {
	private:
		std::vector<types::Type *> inTypes;
		types::Type *outType;
		std::vector<Pipeline> pipelines;
		llvm::Value *result;

		std::vector<std::string> argNames;
		std::map<std::string, Var *> argVars;

		bool gen;
		llvm::Value *promise;
		llvm::Value *handle;
		llvm::BasicBlock *cleanup;
		llvm::BasicBlock *suspend;

		std::string name;
		void *rawFunc;
	public:
		Func(std::string name,
		     std::vector<std::string> argNames,
		     std::vector<types::Type *> inTypes,
		     types::Type *outType);
		Func(types::Type& inType,
		     types::Type& outType,
		     std::string name,
		     void *rawFunc);
		Func(types::Type& inType, types::Type& outType);

		void setGen();
		void codegen(llvm::Module *module) override;
		llvm::Value *codegenCall(BaseFunc *base,
		                         std::vector<llvm::Value *> args,
		                         llvm::BasicBlock *block) override;
		void codegenReturn(llvm::Value *val,
		                   types::Type *type,
		                   llvm::BasicBlock*& block) override;
		void codegenYield(llvm::Value *val,
		                  types::Type *type,
		                  llvm::BasicBlock*& block) override;
		void add(Pipeline pipeline) override;
		void finalize(llvm::Module *module, llvm::ExecutionEngine *eng);

		bool singleInput() const;
		Var *getArgVar(std::string name);
		types::Type *getInType() const override;
		std::vector<types::Type *> getInTypes() const;
		types::Type *getOutType() const override;
		void setIns(std::vector<types::Type *> inTypes);
		void setOut(types::Type *outType);
		void setName(std::string name);
		void setArgNames(std::vector<std::string> argNames);
		void setNative(std::string name, void *rawFunc);

		Pipeline operator|(Pipeline to);
		Pipeline operator|(PipelineList& to);

		Call& operator()();

		Func *clone(types::RefType *ref) override;
	};

	class BaseFuncLite : public BaseFunc {
	public:
		explicit BaseFuncLite(llvm::Function *func);

		void codegen(llvm::Module *module) override;
		llvm::Value *codegenCall(BaseFunc *base,
		                         std::vector<llvm::Value *> args,
		                         llvm::BasicBlock *block) override;
		void codegenReturn(llvm::Value *val,
		                   types::Type *type,
		                   llvm::BasicBlock*& block) override;
		void codegenYield(llvm::Value *val,
		                  types::Type *type,
		                  llvm::BasicBlock*& block) override;
		void add(Pipeline pipeline) override;
	};

}

#endif /* SEQ_FUNC_H */
