#ifndef SEQ_EXPR_H
#define SEQ_EXPR_H

#include "patterns.h"
#include "types.h"
#include "ref.h"
#include "ops.h"
#include "common.h"

namespace seq {

	/**
	 * Class from which all Seq expressions derive.
	 *
	 * An "expression" can be thought of as an AST, potentially
	 * containing subexpressions. Every expression has a type.
	 */
	class Expr : public SrcObject {
	private:
		types::Type *type;
		TryCatch *tc;
	protected:
		std::string name;
	public:
		explicit Expr(types::Type *type);
		Expr();
		void setTryCatch(TryCatch *tc);
		TryCatch *getTryCatch();
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block);
		types::Type *getType() const;
		std::string getName() const;
		virtual void resolveTypes();
		virtual llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block)=0;
		virtual types::Type *getType0() const;
		virtual void ensure(types::Type *type);
		virtual Expr *clone(Generic *ref);
	};

	class BlankExpr : public Expr {
	public:
		BlankExpr();
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
	};

	class NoneExpr : public Expr {
	public:
		NoneExpr();
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
	};

	class TypeExpr : public Expr {
	public:
		explicit TypeExpr(types::Type *type);
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
	};

	class ValueExpr : public Expr {
	private:
		llvm::Value *val;
	public:
		ValueExpr(types::Type *type, llvm::Value *val);
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
	};

	class IntExpr : public Expr {
	private:
		seq_int_t n;
	public:
		explicit IntExpr(seq_int_t n);
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		seq_int_t value() const;
	};

	class FloatExpr : public Expr {
	private:
		double f;
	public:
		explicit FloatExpr(double f);
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
	};

	class BoolExpr : public Expr {
	private:
		bool b;
	public:
		explicit BoolExpr(bool b);
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
	};

	class StrExpr : public Expr {
	private:
		std::string s;
		llvm::GlobalVariable *strVar;
	public:
		explicit StrExpr(std::string s);
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
	};

	class SeqExpr : public Expr {
	private:
		std::string s;
	public:
		explicit SeqExpr(std::string s);
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
	};

	class ListExpr : public Expr {
	private:
		std::vector<Expr *> elems;
		types::Type *listType;
	public:
		ListExpr(std::vector<Expr *> elems, types::Type *listType);
		void resolveTypes() override;
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
		ListExpr *clone(Generic *ref) override;
	};

	class SetExpr : public Expr {
	private:
		std::vector<Expr *> elems;
		types::Type *setType;
	public:
		SetExpr(std::vector<Expr *> elems, types::Type *setType);
		void resolveTypes() override;
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
		SetExpr *clone(Generic *ref) override;
	};

	class DictExpr : public Expr {
	private:
		std::vector<Expr *> elems;
		types::Type *dictType;
	public:
		DictExpr(std::vector<Expr *> elems, types::Type *dictType);
		void resolveTypes() override;
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
		DictExpr *clone(Generic *ref) override;
	};

	class For;

	class ListCompExpr : public Expr {
	private:
		Expr *val;
		For *body;
		types::Type *listType;
		bool realize;
	public:
		ListCompExpr(Expr *val, For *body, types::Type *listType, bool realize=true);
		void setBody(For *body);
		void resolveTypes() override;
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
		ListCompExpr *clone(Generic *ref) override;
	};

	class SetCompExpr : public Expr {
	private:
		Expr *val;
		For *body;
		types::Type *setType;
		bool realize;
	public:
		SetCompExpr(Expr *val, For *body, types::Type *setType, bool realize=true);
		void setBody(For *body);
		void resolveTypes() override;
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
		SetCompExpr *clone(Generic *ref) override;
	};

	class DictCompExpr : public Expr {
	private:
		Expr *key;
		Expr *val;
		For *body;
		types::Type *dictType;
		bool realize;
	public:
		DictCompExpr(Expr *key, Expr *val, For *body, types::Type *dictType, bool realize=true);
		void setBody(For *body);
		void resolveTypes() override;
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
		DictCompExpr *clone(Generic *ref) override;
	};

	class GenExpr : public Expr {
	private:
		Expr *val;
		For *body;
		std::vector<Var *> captures;
	public:
		GenExpr(Expr *val, For *body, std::vector<Var *> captures);
		void setBody(For *body);
		void resolveTypes() override;
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
		GenExpr *clone(Generic *ref) override;
	};

	class VarExpr : public Expr {
	private:
		Var *var;
	public:
		explicit VarExpr(Var *var);
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
		VarExpr *clone(Generic *ref) override;
	};

	class VarPtrExpr : public Expr {
	private:
		Var *var;
	public:
		explicit VarPtrExpr(Var *var);
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
		VarPtrExpr *clone(Generic *ref) override;
	};

	class FuncExpr : public Expr {
	private:
		BaseFunc *func;
		std::vector<types::Type *> types;
		Expr *orig;  // original expression before type deduction, if any
	public:
		FuncExpr(BaseFunc *func, Expr *orig, std::vector<types::Type *> types={});
		explicit FuncExpr(BaseFunc *func, std::vector<types::Type *> types={});
		BaseFunc *getFunc();
		bool isRealized() const;
		void setRealizeTypes(std::vector<types::Type *> types);
		void resolveTypes() override;
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
		Expr *clone(Generic *ref) override;
	};

	class ArrayExpr : public Expr {
	private:
		Expr *count;
	public:
		ArrayExpr(types::Type *type, Expr *count);
		void resolveTypes() override;
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		ArrayExpr *clone(Generic *ref) override;
	};

	class RecordExpr : public Expr {
	private:
		std::vector<Expr *> exprs;
		std::vector<std::string> names;
	public:
		explicit RecordExpr(std::vector<Expr *> exprs, std::vector<std::string> names={});
		void resolveTypes() override;
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
		RecordExpr *clone(Generic *ref) override;
	};

	class IsExpr : public Expr {
	private:
		Expr *lhs;
		Expr *rhs;
	public:
		IsExpr(Expr *lhs, Expr *rhs);
		void resolveTypes() override;
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
		IsExpr *clone(Generic *ref) override;
	};

	class UOpExpr : public Expr {
	private:
		Op op;
		Expr *lhs;
	public:
		UOpExpr(Op op, Expr *lhs);
		void resolveTypes() override;
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
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
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
		BOpExpr *clone(Generic *ref) override;
	};

	class ArrayLookupExpr : public Expr {
	private:
		Expr *arr;
		Expr *idx;
	public:
		ArrayLookupExpr(Expr *arr, Expr *idx);
		void resolveTypes() override;
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
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
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
		ArraySliceExpr *clone(Generic *ref) override;
	};

	class ArrayContainsExpr : public Expr {
	private:
		Expr *val;
		Expr *arr;
	public:
		ArrayContainsExpr(Expr *val, Expr *arr);
		void resolveTypes() override;
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
		ArrayContainsExpr *clone(Generic *ref) override;
	};

	class GetElemExpr : public Expr {
	private:
		Expr *rec;
		std::string memb;
		std::vector<types::Type *> types;
		GetElemExpr *orig;
	public:
		GetElemExpr(Expr *rec,
		            std::string memb,
		            GetElemExpr *orig,
		            std::vector<types::Type *> types={});
		GetElemExpr(Expr *rec,
		            std::string memb,
		            std::vector<types::Type *> types={});
		GetElemExpr(Expr *rec,
		            unsigned memb,
		            std::vector<types::Type *> types={});
		Expr *getRec();
		std::string getMemb();
		bool isRealized() const;
		void setRealizeTypes(std::vector<types::Type *> types);
		void resolveTypes() override;
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
		GetElemExpr *clone(Generic *ref) override;
	};

	class GetStaticElemExpr : public Expr {
	private:
		types::Type *type;
		std::string memb;
		std::vector<types::Type *> types;
		GetStaticElemExpr *orig;
	public:
		GetStaticElemExpr(types::Type *type,
		                  std::string memb,
		                  GetStaticElemExpr *orig,
		                  std::vector<types::Type *> types={});
		GetStaticElemExpr(types::Type *type,
		                  std::string memb,
		                  std::vector<types::Type *> types={});
		types::Type *getTypeInExpr() const;
		std::string getMemb() const;
		bool isRealized() const;
		void setRealizeTypes(std::vector<types::Type *> types);
		void resolveTypes() override;
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
		GetStaticElemExpr *clone(Generic *ref) override;
	};

	class CallExpr : public Expr {
	private:
		mutable Expr *func;
		std::vector<Expr *> args;
	public:
		CallExpr(Expr *func, std::vector<Expr *> args);
		Expr *getFuncExpr() const;
		void setFuncExpr(Expr *func);
		void resolveTypes() override;
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
		CallExpr *clone(Generic *ref) override;
	};

	class PartialCallExpr : public Expr {
	private:
		mutable Expr *func;
		std::vector<Expr *> args;
	public:
		PartialCallExpr(Expr *func, std::vector<Expr *> args);
		Expr *getFuncExpr() const;
		void setFuncExpr(Expr *func);
		void resolveTypes() override;
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::PartialFuncType *getType0() const override;
		PartialCallExpr *clone(Generic *ref) override;
	};

	class CondExpr : public Expr {
	private:
		Expr *cond;
		Expr *ifTrue;
		Expr *ifFalse;
	public:
		CondExpr(Expr *cond, Expr *ifTrue, Expr *ifFalse);
		void resolveTypes() override;
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
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
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
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
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
		ConstructExpr *clone(Generic *ref) override;
	};

	class OptExpr : public Expr {
	private:
		Expr *val;
	public:
		explicit OptExpr(Expr *val);
		void resolveTypes() override;
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
		OptExpr *clone(Generic *ref) override;
	};

	class DefaultExpr : public Expr {
	public:
		explicit DefaultExpr(types::Type *type);
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		DefaultExpr *clone(Generic *ref) override;
	};

	class TypeOfExpr : public Expr {
	private:
		Expr *val;
	public:
		explicit TypeOfExpr(Expr *val);
		void resolveTypes() override;
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		TypeOfExpr *clone(Generic *ref) override;
	};

	class PipeExpr : public Expr {
	private:
		std::vector<Expr *> stages;
	public:
		explicit PipeExpr(std::vector<Expr *> stages);
		void resolveTypes() override;
		llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType0() const override;
		PipeExpr *clone(Generic *ref) override;
	};

}

#endif /* SEQ_EXPR_H */
