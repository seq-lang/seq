#pragma once

#include "lang/ops.h"
#include "lang/patterns.h"
#include "types/ref.h"
#include "types/types.h"
#include "util/common.h"

namespace seq {
/**
 * Class from which all Seq expressions derive.
 *
 * An "expression" can be thought of as an AST node,
 * potentially containing sub-expressions. Every
 * expression has a type.
 */
class Expr : public SrcObject {
private:
  /// Expression type if fixed, or null if not
  types::Type *type;

  /// Enclosing try-catch, or null of none
  TryCatch *tc;

protected:
  /// Human-readable name for this expression,
  /// mainly for debugging
  std::string name;

public:
  /// Constructs an expression of fixed type.
  /// @param type fixed type of expression
  explicit Expr(types::Type *type);

  /// Constructs an expression without a fixed type.
  Expr();

  /// Sets the enclosing try-catch statement.
  void setTryCatch(TryCatch *tc);

  /// Returns the enclosing try-catch statement.
  TryCatch *getTryCatch();

  /// Delegates to \ref codegen0() "codegen0()"; catches
  /// exceptions and fills in source information.
  llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock *&block);

  /// Returns the type of this expression.
  types::Type *getType() const;

  /// Sets the type of this expression.
  void setType(types::Type *type);

  /// Returns the given name of this expression.
  std::string getName() const;

  /// Performs code generation for this expression.
  /// @param base the function containing this expression
  /// @param block reference to block where code should be
  ///              generated; possibly modified to point
  ///              to a new block where codegen should resume
  /// @return value representing expression result; possibly
  ///         null if type is void
  virtual llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) = 0;

  /// Ensures that this expression has the specified type.
  /// Throws an exception if this is not the case.
  /// @param type required type of this expression
  virtual void ensure(types::Type *type);
};

struct Const {
  enum Type { NONE, INT, FLOAT, BOOL, STR, SEQ };
  Type type;
  seq_int_t ival;
  double fval;
  bool bval;
  std::string sval;
  Const() : type(Type::NONE), ival(0), fval(0.0), bval(false), sval() {}
  explicit Const(seq_int_t ival)
      : type(Type::INT), ival(ival), fval(0.0), bval(false), sval() {}
  explicit Const(double fval)
      : type(Type::FLOAT), ival(0), fval(fval), bval(false), sval() {}
  explicit Const(bool bval)
      : type(Type::BOOL), ival(0), fval(0.0), bval(bval), sval() {}
  explicit Const(std::string sval, bool seq = false)
      : type(seq ? Type::SEQ : Type::STR), ival(0), fval(0.0), bval(false),
        sval(std::move(sval)) {}
};

class ConstExpr : public Expr {
public:
  explicit ConstExpr(types::Type *type) : Expr(type) {}
  virtual Const constValue() const = 0;
};

class TypeExpr : public Expr {
public:
  explicit TypeExpr(types::Type *type);
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
};

class ValueExpr : public Expr {
private:
  llvm::Value *val;

public:
  ValueExpr(types::Type *type, llvm::Value *val);
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
};

class NoneExpr : public ConstExpr {
public:
  NoneExpr();
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
  Const constValue() const override;
};

class IntExpr : public ConstExpr {
private:
  seq_int_t n;

public:
  explicit IntExpr(seq_int_t n);
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
  Const constValue() const override;
  seq_int_t value() const;
};

class FloatExpr : public ConstExpr {
private:
  double f;

public:
  explicit FloatExpr(double f);
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
  Const constValue() const override;
  double value() const;
};

class BoolExpr : public ConstExpr {
private:
  bool b;

public:
  explicit BoolExpr(bool b);
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
  Const constValue() const override;
  bool value() const;
};

class StrExpr : public ConstExpr {
private:
  std::string s;

public:
  explicit StrExpr(std::string s);
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
  Const constValue() const override;
  std::string value() const;
};

class SeqExpr : public ConstExpr {
private:
  std::string s;

public:
  explicit SeqExpr(std::string s);
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
  Const constValue() const override;
  std::string value() const;
};

class VarExpr : public Expr {
private:
  Var *var;
  bool atomic;

public:
  explicit VarExpr(Var *var, bool atomic = false);
  Var *getVar();
  void setAtomic();
  Var *getVar() const;
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
};

class VarPtrExpr : public Expr {
private:
  Var *var;

public:
  explicit VarPtrExpr(Var *var);
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
};

class FuncExpr : public Expr {
private:
  BaseFunc *func;

public:
  explicit FuncExpr(BaseFunc *func);
  BaseFunc *getFunc();
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
};

class ArrayExpr : public Expr {
private:
  Expr *count;
  bool doAlloca;

public:
  ArrayExpr(types::Type *type, Expr *count, bool doAlloca = false);
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
};

class RecordExpr : public Expr {
private:
  std::vector<Expr *> exprs;
  std::vector<std::string> names;

public:
  explicit RecordExpr(std::vector<Expr *> exprs,
                      std::vector<std::string> names = {});
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
};

class IsExpr : public Expr {
private:
  Expr *lhs;
  Expr *rhs;

public:
  IsExpr(Expr *lhs, Expr *rhs);
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
};

class UOpExpr : public Expr {
private:
  Op op;
  Expr *lhs;

public:
  UOpExpr(Op op, Expr *lhs);
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
};

class BOpExpr : public Expr {
private:
  Op op;
  Expr *lhs;
  Expr *rhs;
  bool inPlace;

public:
  BOpExpr(Op op, Expr *lhs, Expr *rhs, bool inPlace = false);
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
};

class AtomicExpr : public Expr {
public:
  enum Op {
    XCHG,
    ADD,
    SUB,
    AND,
    NAND,
    OR,
    XOR,
    MAX,
    MIN,
  };

private:
  AtomicExpr::Op op;
  Var *lhs;
  Expr *rhs;

public:
  AtomicExpr(AtomicExpr::Op op, Var *lhs, Expr *rhs);
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
};

class ArrayLookupExpr : public Expr {
private:
  Expr *arr;
  Expr *idx;

public:
  ArrayLookupExpr(Expr *arr, Expr *idx);
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
};

class ArrayContainsExpr : public Expr {
private:
  Expr *val;
  Expr *arr;

public:
  ArrayContainsExpr(Expr *val, Expr *arr);
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
};

class GetElemExpr : public Expr {
private:
  Expr *rec;
  std::string memb;

public:
  GetElemExpr(Expr *rec, std::string memb);
  GetElemExpr(Expr *rec, unsigned memb);
  Expr *getRec();
  std::string getMemb();
  bool isRealized() const;
  void setRealizeTypes(std::vector<types::Type *> types);
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
};

class GetStaticElemExpr : public Expr {
private:
  types::Type *type;
  std::string memb;

public:
  GetStaticElemExpr(types::Type *type, std::string memb);
  types::Type *getTypeInExpr() const;
  std::string getMemb() const;
  bool isRealized() const;
  void setRealizeTypes(std::vector<types::Type *> types);
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
};

class CallExpr : public Expr {
private:
  Expr *func;
  std::vector<Expr *> args;
  std::vector<std::string> names;

public:
  CallExpr(Expr *func, std::vector<Expr *> args,
           std::vector<std::string> names = {});
  Expr *getFuncExpr() const;
  std::vector<Expr *> getArgs() const;
  void setFuncExpr(Expr *func);
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
};

class PartialCallExpr : public Expr {
private:
  Expr *func;
  std::vector<Expr *> args;
  std::vector<std::string> names;

public:
  PartialCallExpr(Expr *func, std::vector<Expr *> args,
                  std::vector<std::string> names = {});
  Expr *getFuncExpr() const;
  std::vector<Expr *> getArgs() const;
  void setFuncExpr(Expr *func);
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
};

class CondExpr : public Expr {
private:
  Expr *cond;
  Expr *ifTrue;
  Expr *ifFalse;

public:
  CondExpr(Expr *cond, Expr *ifTrue, Expr *ifFalse);
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
};

class MatchExpr : public Expr {
private:
  Expr *value;
  std::vector<Pattern *> patterns;
  std::vector<Expr *> exprs;

public:
  MatchExpr();
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
  void setValue(Expr *value);
  void addCase(Pattern *pattern, Expr *expr);
};

class ConstructExpr : public Expr {
private:
  types::Type *type;
  std::vector<Expr *> args;
  std::vector<std::string> names;

public:
  ConstructExpr(types::Type *type, std::vector<Expr *> args,
                std::vector<std::string> names = {});
  types::Type *getConstructType();
  std::vector<Expr *> getArgs();
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
};

class MethodExpr : public Expr {
private:
  Expr *self;
  Func *func;

public:
  MethodExpr(Expr *self, Func *method);
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
};

class OptExpr : public Expr {
private:
  Expr *val;

public:
  explicit OptExpr(Expr *val);
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
};

class YieldExpr : public Expr {
private:
  BaseFunc *base;

public:
  YieldExpr(BaseFunc *base);
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
};

class DefaultExpr : public Expr {
public:
  explicit DefaultExpr(types::Type *type);
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
};

class TypeOfExpr : public Expr {
private:
  Expr *val;

public:
  explicit TypeOfExpr(Expr *val);
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
};

} // namespace seq
