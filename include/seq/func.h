#ifndef SEQ_FUNC_H
#define SEQ_FUNC_H

#include "stmt.h"
#include "types.h"
#include "funct.h"
#include "generic.h"
#include "common.h"

namespace seq {

	class Expr;
	class Var;
	class Return;
	class Yield;

	class BaseFunc {
	protected:
		llvm::Module *module;
		llvm::BasicBlock *preambleBlock;
		llvm::Function *func;
		BaseFunc();
	public:
		virtual void resolveTypes();
		virtual void codegen(llvm::Module *module)=0;
		llvm::LLVMContext& getContext();
		llvm::BasicBlock *getPreamble() const;
		virtual types::FuncType *getFuncType() const;
		virtual llvm::Function *getFunc();
		virtual BaseFunc *clone(Generic *ref);
	};

	class Func : public BaseFunc, public Generic, public SrcObject {
	private:
		std::string name;
		std::vector<types::Type *> inTypes;
		types::Type *outType;
		Block *scope;

		std::vector<std::string> argNames;
		std::map<std::string, Var *> argVars;

		Return *ret;
		Yield *yield;
		bool resolvingTypes;  // make sure we don't keep resolving recursively

		bool gen;
		llvm::Value *promise;
		llvm::Value *handle;
		llvm::BasicBlock *cleanup;
		llvm::BasicBlock *suspend;
		llvm::BasicBlock *exit;
	public:
		Func();
		Block *getBlock();

		std::string genericName() override;
		Func *realize(std::vector<types::Type *> types);
		std::vector<types::Type *> deduceTypesFromArgTypes(std::vector<types::Type *> argTypes);

		void sawReturn(Return *ret);
		void sawYield(Yield *yield);

		void resolveTypes() override;
		void codegen(llvm::Module *module) override;
		void codegenReturn(llvm::Value *val,
		                   types::Type *type,
		                   llvm::BasicBlock*& block);
		void codegenYield(llvm::Value *val,
		                  types::Type *type,
		                  llvm::BasicBlock*& block);
		Var *getArgVar(std::string name);
		types::FuncType *getFuncType() const override;
		void setIns(std::vector<types::Type *> inTypes);
		void setOut(types::Type *outType);
		void setName(std::string name);
		void setArgNames(std::vector<std::string> argNames);

		Func *clone(Generic *ref) override;
	};

	class BaseFuncLite : public BaseFunc {
	private:
		std::vector<types::Type *> inTypes;
		types::Type *outType;
		std::function<llvm::Function *(llvm::Module *)> codegenLambda;
	public:
		BaseFuncLite(std::vector<types::Type *> inTypes,
		             types::Type *outType,
		             std::function<llvm::Function *(llvm::Module *)> codegenLambda);
		void codegen(llvm::Module *module) override;
		types::FuncType *getFuncType() const override;
		BaseFuncLite *clone(Generic *ref) override;
	};

}

#endif /* SEQ_FUNC_H */
