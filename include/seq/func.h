#ifndef SEQ_FUNC_H
#define SEQ_FUNC_H

#include "stmt.h"
#include "types.h"
#include "common.h"

namespace seq {

	class Expr;
	class Var;

	class BaseFunc {
	protected:
		llvm::Module *module;
		llvm::BasicBlock *preambleBlock;
		llvm::Function *func;
		BaseFunc();
	public:
		virtual void codegen(llvm::Module *module)=0;
		virtual void codegenReturn(llvm::Value *val,
		                           types::Type *type,
		                           llvm::BasicBlock*& block)=0;
		virtual void codegenYield(llvm::Value *val,
		                          types::Type *type,
		                          llvm::BasicBlock*& block)=0;

		llvm::LLVMContext& getContext();
		llvm::BasicBlock *getPreamble() const;
		virtual types::Type *getInType() const;
		virtual types::Type *getOutType() const;
		virtual types::FuncType *getFuncType() const;
		virtual llvm::Function *getFunc();

		virtual BaseFunc *clone(types::RefType *ref);
	};

	class Func : public BaseFunc {
	private:
		std::string name;
		std::vector<types::Type *> inTypes;
		types::Type *outType;
		Block *scope;

		std::vector<std::string> argNames;
		std::map<std::string, Var *> argVars;

		bool gen;
		llvm::Value *promise;
		llvm::Value *handle;
		llvm::BasicBlock *cleanup;
		llvm::BasicBlock *suspend;
	public:
		Func(std::string name,
		     std::vector<std::string> argNames,
		     std::vector<types::Type *> inTypes,
		     types::Type *outType);

		Block *getBlock();
		void setGen();
		void codegen(llvm::Module *module) override;
		void codegenReturn(llvm::Value *val,
		                   types::Type *type,
		                   llvm::BasicBlock*& block) override;
		void codegenYield(llvm::Value *val,
		                  types::Type *type,
		                  llvm::BasicBlock*& block) override;
		Var *getArgVar(std::string name);
		types::Type *getInType() const override;
		std::vector<types::Type *> getInTypes() const;
		types::Type *getOutType() const override;
		types::FuncType *getFuncType() const override;
		void setIns(std::vector<types::Type *> inTypes);
		void setOut(types::Type *outType);
		void setName(std::string name);
		void setArgNames(std::vector<std::string> argNames);
		Func *clone(types::RefType *ref) override;
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
		void codegenReturn(llvm::Value *val,
		                   types::Type *type,
		                   llvm::BasicBlock*& block) override;
		void codegenYield(llvm::Value *val,
		                  types::Type *type,
		                  llvm::BasicBlock*& block) override;

		types::Type *getInType() const override;
		std::vector<types::Type *> getInTypes() const;
		types::Type *getOutType() const override;
		types::FuncType *getFuncType() const override;

		BaseFuncLite *clone(types::RefType *ref) override;
	};

}

#endif /* SEQ_FUNC_H */
