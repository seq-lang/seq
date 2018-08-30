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
		void resolveTypes() override;
		void codegen0(llvm::BasicBlock*& block) override;
		Source *clone(Generic *ref) override;
	};
}

#endif /* SEQ_SOURCE_H */
