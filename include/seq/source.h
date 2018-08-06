#ifndef SEQ_SOURCE_H
#define SEQ_SOURCE_H

#include <vector>
#include "expr.h"
#include "stage.h"

namespace seq {
	class Source : public Stage {
	private:
		std::vector<Expr *> sources;
		Block *scope;
		Cell *var;
		bool isSingle() const;
		types::Type *determineOutType() const;
	public:
		explicit Source(std::vector<Expr *> sources);
		Block *getBlock();
		Cell *getVar();
		void codegen(llvm::BasicBlock*& block) override;
		static Source& make(std::vector<Expr *>);
		Source *clone(types::RefType *ref) override;
	};
}

#endif /* SEQ_SOURCE_H */
