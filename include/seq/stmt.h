#ifndef SEQ_STMT_H
#define SEQ_STMT_H

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
#include "generic.h"
#include "optional.h"

#include "common.h"

namespace seq {

	class BaseFunc;
	class Func;

	typedef void (*SeqMain)(arr_t<str_t> args);

	class Stmt;
	struct Block {
		Stmt *parent;
		std::vector<Stmt *> stmts;

		explicit Block(Stmt *parent=nullptr);
		void add(Stmt *stmt);
		void codegen(llvm::BasicBlock*& block);
		Block *clone(Generic *ref);
	};

	class Stmt {
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
		explicit Stmt(std::string name);
		std::string getName() const;
		Stmt *getPrev() const;
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
		virtual Stmt *clone(Generic *ref);
		virtual void setCloneBase(Stmt *stmt, Generic *ref);
	};

}

std::ostream& operator<<(std::ostream& os, seq::Stmt& stmt);

#endif /* SEQ_STMT_H */
