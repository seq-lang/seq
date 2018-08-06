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

	typedef void (*SeqMain)(arr_t<str_t> args);

	class Stage;
	struct Block {
		Stage *parent;
		std::vector<Stage *> stmts;

		explicit Block(Stage *parent=nullptr);
		void add(Stage *stmt);
		void codegen(llvm::BasicBlock*& block);
		Block *clone(types::RefType *ref);
	};

	class Stage {
	private:
		BaseFunc *base;

		/* loops */
		std::vector<llvm::BranchInst *> breaks;
		std::vector<llvm::BranchInst *> continues;
	protected:
		Block *parent;
		bool loop;
	public:
		std::string name;
		explicit Stage(std::string name);
		std::string getName() const;
		Stage *getPrev() const;
		void setParent(Block *parent);
		BaseFunc *getBase() const;
		void setBase(BaseFunc *base);
		void addBreakToEnclosingLoop(llvm::BranchInst *inst);
		void addContinueToEnclosingLoop(llvm::BranchInst *inst);

		bool isLoop();
		void ensureLoop();
		void addBreak(llvm::BranchInst *inst);
		void addContinue(llvm::BranchInst *inst);
		void setBreaks(llvm::BasicBlock *block);
		void setContinues(llvm::BasicBlock *block);

		virtual void codegen(llvm::BasicBlock*& block);
		virtual Stage *clone(types::RefType *ref);
		virtual void setCloneBase(Stage *stage, types::RefType *ref);
	};

}

std::ostream& operator<<(std::ostream& os, seq::Stage& stage);

#endif /* SEQ_STAGES_H */
