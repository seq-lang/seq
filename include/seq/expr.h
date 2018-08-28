#ifndef SEQ_EXPR_H
#define SEQ_EXPR_H

#include "patterns.h"
#include "types.h"

namespace seq {

	class Expr {
	private:
		types::Type *type;
	public:
		explicit Expr(types::Type *type);
		Expr();
		virtual void resolveTypes();
		virtual llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block)=0;
		virtual types::Type *getType() const;
		virtual void ensure(types::Type *type);
		virtual Expr *clone(Generic *ref);
	};

	class IntExpr : public Expr {
	private:
		seq_int_t n;
	public:
		explicit IntExpr(seq_int_t n);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
	};

	class FloatExpr : public Expr {
	private:
		double f;
	public:
		explicit FloatExpr(double f);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
	};

	class BoolExpr : public Expr {
	private:
		bool b;
	public:
		explicit BoolExpr(bool b);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
	};

	class StrExpr : public Expr {
	private:
		std::string s;
	public:
		explicit StrExpr(std::string s);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
	};

	class VarExpr : public Expr {
	private:
		Var *var;
	public:
		explicit VarExpr(Var *var);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		VarExpr *clone(Generic *ref) override;
	};

	class FuncExpr : public Expr {
	private:
		BaseFunc *func;
		std::vector<types::Type *> types;
	public:
		explicit FuncExpr(BaseFunc *func, std::vector<types::Type *> types={});
		bool isParameterized();
		BaseFunc *getFunc();
		void resolveTypes() override;
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		FuncExpr *clone(Generic *ref) override;
	};

	class ArrayExpr : public Expr {
	private:
		Expr *count;
	public:
		ArrayExpr(types::Type *type, Expr *count);
		void resolveTypes() override;
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		ArrayExpr *clone(Generic *ref) override;
	};

	class RecordExpr : public Expr {
	private:
		std::vector<Expr *> exprs;
		std::vector<std::string> names;
	public:
		explicit RecordExpr(std::vector<Expr *> exprs, std::vector<std::string> names={});
		void resolveTypes() override;
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		RecordExpr *clone(Generic *ref) override;
	};

	class UOpExpr : public Expr {
	private:
		Op op;
		Expr *lhs;
	public:
		UOpExpr(Op op, Expr *lhs);
		void resolveTypes() override;
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		UOpExpr *clone(Generic *ref) override;
	};

	class BOpExpr : public Expr {
	private:
		Op op;
		Expr *lhs;
		Expr *rhs;
	public:
		BOpExpr(Op op, Expr *lhs, Expr *rhs);
		void resolveTypes() override;
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		BOpExpr *clone(Generic *ref) override;
	};

	class ArrayLookupExpr : public Expr {
	private:
		Expr *arr;
		Expr *idx;
	public:
		ArrayLookupExpr(Expr *arr, Expr *idx);
		void resolveTypes() override;
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		ArrayLookupExpr *clone(Generic *ref) override;
	};

	class ArraySliceExpr : public Expr {
	private:
		Expr *arr;
		Expr *from;
		Expr *to;
	public:
		ArraySliceExpr(Expr *arr, Expr *from, Expr *to);
		void resolveTypes() override;
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		ArraySliceExpr *clone(Generic *ref) override;
	};

	class GetElemExpr : public Expr {
	private:
		Expr *rec;
		std::string memb;
	public:
		GetElemExpr(Expr *rec, std::string memb);
		GetElemExpr(Expr *rec, seq_int_t idx);
		Expr *getRec();
		std::string getMemb();
		void resolveTypes() override;
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		GetElemExpr *clone(Generic *ref) override;
	};

	class GetStaticElemExpr : public Expr {
	private:
		types::Type *type;
		std::string memb;
	public:
		GetStaticElemExpr(types::Type *type, std::string memb);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		GetStaticElemExpr *clone(Generic *ref) override;
	};

	class MethodExpr : public Expr {
	private:
		Expr *expr;
		std::string name;
		std::vector<types::Type *> types;
	public:
		MethodExpr(Expr *expr, std::string method, std::vector<types::Type *> realizedTypes);
		void resolveTypes() override;
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::MethodType *getType() const override;
		MethodExpr *clone(Generic *ref) override;
	};

	class CallExpr : public Expr {
	private:
		mutable Expr *func;
		std::vector<Expr *> args;
	public:
		CallExpr(Expr *func, std::vector<Expr *> args);
		void resolveTypes() override;
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		CallExpr *clone(Generic *ref) override;
	};

	class CondExpr : public Expr {
	private:
		Expr *cond;
		Expr *ifTrue;
		Expr *ifFalse;
	public:
		CondExpr(Expr *cond, Expr *ifTrue, Expr *ifFalse);
		void resolveTypes() override;
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		CondExpr *clone(Generic *ref) override;
	};

	class MatchExpr : public Expr {
	private:
		Expr *value;
		std::vector<Pattern *> patterns;
		std::vector<Expr *> exprs;
	public:
		MatchExpr();
		void resolveTypes() override;
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		MatchExpr *clone(Generic *ref) override;
		void setValue(Expr *value);
		void addCase(Pattern *pattern, Expr *expr);
	};

	class ConstructExpr : public Expr {
	private:
		mutable types::Type *type;
		std::vector<Expr *> args;
	public:
		ConstructExpr(types::Type *type, std::vector<Expr *> args);
		void resolveTypes() override;
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		ConstructExpr *clone(Generic *ref) override;
	};

	class OptExpr : public Expr {
	private:
		Expr *val;
	public:
		explicit OptExpr(Expr *val);
		void resolveTypes() override;
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		OptExpr *clone(Generic *ref) override;
	};

	class DefaultExpr : public Expr {
	public:
		explicit DefaultExpr(types::Type *type);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		DefaultExpr *clone(Generic *ref) override;
	};

}

#endif /* SEQ_EXPR_H */
