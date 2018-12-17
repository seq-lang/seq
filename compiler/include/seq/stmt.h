#ifndef SEQ_STMT_H
#define SEQ_STMT_H

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include "generic.h"
#include "common.h"
#include "llvm.h"

namespace seq {

	class BaseFunc;
	class Func;
	class TryCatch;

	class Stmt;
	struct Block {
		Stmt *parent;
		std::vector<Stmt *> stmts;

		explicit Block(Stmt *parent=nullptr);
		void add(Stmt *stmt);
		void resolveTypes();
		void codegen(llvm::BasicBlock*& block);
		Block *clone(Generic *ref);
	};

	class Stmt : public SrcObject {
	private:
		std::string name;
		BaseFunc *base;

		/* loops */
		std::vector<llvm::BranchInst *> breaks;
		std::vector<llvm::BranchInst *> continues;
	protected:
		Block *parent;
		bool loop;
	public:
		explicit Stmt(std::string name);
		std::string getName() const;
		Stmt *getPrev() const;
		void setParent(Block *parent);
		BaseFunc *getBase() const;
		void setBase(BaseFunc *base);
		void addBreakToEnclosingLoop(llvm::BranchInst *inst);
		void addContinueToEnclosingLoop(llvm::BranchInst *inst);
		void setTryCatch(TryCatch *tc);
		TryCatch *getTryCatch();

		bool isLoop();
		void ensureLoop();
		void addBreak(llvm::BranchInst *inst);
		void addContinue(llvm::BranchInst *inst);
		void setBreaks(llvm::BasicBlock *block);
		void setContinues(llvm::BasicBlock *block);

		void codegen(llvm::BasicBlock*& block);
		virtual void resolveTypes();
		virtual void codegen0(llvm::BasicBlock*& block)=0;
		virtual Stmt *clone(Generic *ref)=0;
		virtual void setCloneBase(Stmt *stmt, Generic *ref);
	};

}

std::ostream& operator<<(std::ostream& os, seq::Stmt& stmt);

#endif /* SEQ_STMT_H */
