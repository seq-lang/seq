#ifndef SEQ_GETELEMEXPR_H
#define SEQ_GETELEMEXPR_H

#include "expr.h"

namespace seq {

	class GetElemExpr : public Expr {
	private:
		Expr *rec;
		std::string memb;
	public:
		GetElemExpr(Expr *rec, std::string memb);
		GetElemExpr(Expr *rec, seq_int_t idx);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		GetElemExpr *clone(types::RefType *ref) override;
	};

	class GetStaticElemExpr : public Expr {
	private:
		types::Type *type;
		std::string memb;
	public:
		GetStaticElemExpr(types::Type *type, std::string memb);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		GetStaticElemExpr *clone(types::RefType *ref) override;
	};

}

#endif /* SEQ_GETELEMEXPR_H */
