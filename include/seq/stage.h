#ifndef SEQ_STAGES_H
#define SEQ_STAGES_H

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <map>

#include "llvm.h"
#include "seqdata.h"

#include "types.h"
#include "any.h"
#include "base.h"
#include "void.h"
#include "seqt.h"
#include "num.h"
#include "array.h"
#include "record.h"
#include "funct.h"
#include "ref.h"
#include "optional.h"

#include "common.h"

namespace seq {

	class BaseFunc;
	class Func;
	class Pipeline;
	class PipelineList;
	class Var;

	typedef void (*SeqMain)(seq_t *, bool isLast);
	typedef void (*SeqMainStandalone)(arr_t<str_t> args);
	typedef void (*SeqOp)(char *, seq_int_t);
	typedef seq_int_t (*SeqHash)(char *, seq_int_t);

	class Stage {
	private:
		BaseFunc *base;
		bool added;

		/* loops */
		std::vector<llvm::BranchInst *> breaks;
		std::vector<llvm::BranchInst *> continues;
	protected:
		types::Type *in;
		types::Type *out;
		Stage *prev;
		std::vector<Stage *> nexts;
		std::vector<Stage *> weakNexts;  // for variable references
		bool loop;
		bool init;
	public:
		std::string name;
		llvm::BasicBlock *block;
		llvm::BasicBlock *after;
		llvm::Value *result;

		/* initializations */
		llvm::BasicBlock *initBlock;
		llvm::BasicBlock *startBlock;

		Stage(std::string name, types::Type *in, types::Type *out);
		explicit Stage(std::string name);

		std::string getName() const;
		Stage *getPrev() const;
		virtual void setPrev(Stage *prev);
		std::vector<Stage *>& getNext();
		std::vector<Stage *>& getWeakNext();
		BaseFunc *getBase() const;
		void setBase(BaseFunc *base);
		virtual types::Type *getInType() const;
		virtual types::Type *getOutType() const;
		virtual void setInOut(types::Type *in, types::Type *out);
		virtual void addNext(Stage *next);
		virtual void addWeakNext(Stage *next);
		virtual llvm::BasicBlock *getAfter() const;
		virtual void setAfter(llvm::BasicBlock *block);
		bool isAdded() const;
		void setAdded();
		llvm::BasicBlock *getEnclosingInitBlock();
		void addBreakToEnclosingLoop(llvm::BranchInst *inst);
		void addContinueToEnclosingLoop(llvm::BranchInst *inst);

		bool isLoop();
		void ensureLoop();
		void addBreak(llvm::BranchInst *inst);
		void addContinue(llvm::BranchInst *inst);
		void setBreaks(llvm::BasicBlock *block);
		void setContinues(llvm::BasicBlock *block);

		bool isInit();
		void ensureInit();
		llvm::BasicBlock *getInitBlock();
		void codegenInit(llvm::BasicBlock*& block);
		void finalizeInit();

		virtual void validate();
		virtual void ensurePrev();
		virtual void codegen(llvm::Module *module);
		virtual void codegenNext(llvm::Module *module);
		virtual void finalize(llvm::Module *module, llvm::ExecutionEngine *eng);

		virtual Pipeline operator|(Pipeline to);
		operator Pipeline();

		virtual Stage *clone(types::RefType *ref);
		virtual void setCloneBase(Stage *stage, types::RefType *ref);
	};

	class Nop : public Stage {
	public:
		Nop();
		void validate() override;
		void codegen(llvm::Module *module) override;
		static Nop& make();

		Nop *clone(types::RefType *ref) override;
	};

}

std::ostream& operator<<(std::ostream& os, seq::Stage& stage);

#endif /* SEQ_STAGES_H */
