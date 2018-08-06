#ifndef SEQ_SOURCE_H
#define SEQ_SOURCE_H

#include <vector>
#include "expr.h"
#include "stmt.h"

namespace seq {
	class Source : public Stmt {
	private:
		std::vector<Expr *> sources;
		Block *scope;
		Var *var;
		bool isSingle() const;
		types::Type *determineOutType() const;
	public:
		explicit Source(std::vector<Expr *> sources);
		Block *getBlock();
		Var *getVar();
		void codegen(llvm::BasicBlock*& block) override;
		Source *clone(types::RefType *ref) override;
	};
}

#endif /* SEQ_SOURCE_H */
