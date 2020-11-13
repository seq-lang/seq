/**
 * ast.h
 * Describes Seq AST.
 */

#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/ast/stmt.h"
#include "parser/ast/ast/visitor.h"
#include "parser/ast/types.h"
#include "parser/common.h"

namespace seq {
namespace ast {

struct Expr : public seq::SrcObject {
private:
  /// Each expression comes with an associated type.
  /// Types are nullptr until realized by a typechecker.
  types::TypePtr type;

  /// Flag that indicates is this expression a type expression
  /// (e.g. int, list[int], or generic T)
  bool isTypeExpr;

public:
  Expr();
  Expr(const Expr &e);
  virtual ~Expr();
  virtual unique_ptr<Expr> clone() const = 0;

  /// Convert node to a string
  virtual string toString() const = 0;
  /// Accept an AST walker/visitor
  virtual void accept(ASTVisitor &) const = 0;

  /// Type utilities
  types::TypePtr getType() const;
  void setType(types::TypePtr t);
  bool isType() const;
  void markType();
  string wrap(const string &) const;

  /// Allow pretty-printing to C++ streams
  friend std::ostream &operator<<(std::ostream &out, const Expr &c) {
    return out << c.toString();
  }
};
typedef unique_ptr<Expr> ExprPtr;

/// Type that models the function parameters (name: type = deflt)
struct Param {
  string name;
  ExprPtr type;
  ExprPtr deflt;

  Param(string name = "", ExprPtr type = nullptr, ExprPtr deflt = nullptr);
  Param clone() const;

  string toString() const;
};

struct NoneExpr : public Expr {
  NoneExpr();
  NoneExpr(const NoneExpr &e);

  string toString() const override;
  NODE_UTILITY(Expr, NoneExpr);
};

struct BoolExpr : public Expr {
  bool value;

  BoolExpr(bool v);
  BoolExpr(const BoolExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, BoolExpr);
};

struct IntExpr : public Expr {
  string value;
  /// Number suffix (e.g. "u" for "123u")
  string suffix;

  int64_t intValue;
  bool sign;

  IntExpr(int v);
  IntExpr(string v, string s = "");
  IntExpr(const IntExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, IntExpr);
};

struct FloatExpr : public Expr {
  double value;
  /// Number suffix (e.g. "u" for "123u")
  string suffix;

  FloatExpr(double v, string s = "");
  FloatExpr(const FloatExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, FloatExpr);
};

struct StringExpr : public Expr {
  string prefix;
  string value;

  StringExpr(string v = "", string prefix = "");
  StringExpr(const StringExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, StringExpr);
};

struct IdExpr : public Expr {
  string value;

  IdExpr(string v);
  IdExpr(const IdExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, IdExpr);
};

struct StarExpr : public Expr {
  ExprPtr what;

  StarExpr(ExprPtr w);
  StarExpr(const StarExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, StarExpr);
};

struct TupleExpr : public Expr {
  vector<ExprPtr> items;

  TupleExpr(vector<ExprPtr> &&i);
  TupleExpr(const TupleExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, TupleExpr);
};

struct ListExpr : public Expr {
  vector<ExprPtr> items;

  ListExpr(vector<ExprPtr> &&i);
  ListExpr(const ListExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, ListExpr);
};

struct SetExpr : public Expr {
  vector<ExprPtr> items;

  SetExpr(vector<ExprPtr> &&i);
  SetExpr(const SetExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, SetExpr);
};

struct DictExpr : public Expr {
  struct DictItem {
    ExprPtr key, value;

    DictItem clone() const;
  };
  vector<DictItem> items;

  DictExpr(vector<DictItem> &&it);
  DictExpr(const DictExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, DictExpr);
};

struct GeneratorBody {
  ExprPtr vars;
  ExprPtr gen;
  vector<ExprPtr> conds;

  GeneratorBody clone() const;
};

struct GeneratorExpr : public Expr {
  /// Generator expression: [expr (loops...)]
  /// where loops are: for vars... in gen (if conds...)?
  enum GeneratorKind { Generator, ListGenerator, SetGenerator };

  GeneratorKind kind;
  ExprPtr expr;
  vector<GeneratorBody> loops;

  GeneratorExpr(GeneratorKind k, ExprPtr e, vector<GeneratorBody> &&l);
  GeneratorExpr(const GeneratorExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, GeneratorExpr);
};

struct DictGeneratorExpr : public Expr {
  /// Dictionary generator expression: {key: expr (loops...)}
  /// where loops are: for vars... in gen (if conds...)?
  ExprPtr key, expr;
  vector<GeneratorBody> loops;

  DictGeneratorExpr(ExprPtr k, ExprPtr e, vector<GeneratorBody> &&l);
  DictGeneratorExpr(const DictGeneratorExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, DictGeneratorExpr);
};

struct IfExpr : public Expr {
  ExprPtr cond, eif, eelse;

  IfExpr(ExprPtr c, ExprPtr i, ExprPtr e);
  IfExpr(const IfExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, IfExpr);
};

struct UnaryExpr : public Expr {
  string op;
  ExprPtr expr;

  UnaryExpr(string o, ExprPtr e);
  UnaryExpr(const UnaryExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, UnaryExpr);
};

struct BinaryExpr : public Expr {
  string op;
  ExprPtr lexpr, rexpr;
  /// Does this expression modify lhs (e.g. a += b)?
  bool inPlace;

  BinaryExpr(ExprPtr l, string o, ExprPtr r, bool i = false);
  BinaryExpr(const BinaryExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, BinaryExpr);
};

struct PipeExpr : public Expr {
  /// Pipe expression: [op expr]...
  /// The first item has op = ""; others have op = "|>" or op = "||>"

  struct Pipe {
    string op;
    ExprPtr expr;

    Pipe clone() const;
  };

  vector<Pipe> items;
  vector<types::TypePtr> inTypes;

  PipeExpr(vector<Pipe> &&it);
  PipeExpr(const PipeExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, PipeExpr);
};

struct IndexExpr : public Expr {
  ExprPtr expr, index;

  IndexExpr(ExprPtr e, ExprPtr i);
  IndexExpr(const IndexExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, IndexExpr);
};

struct CallExpr : public Expr {
  /// Each argument can have a name (e.g. foo(1, b=5))
  struct Arg {
    string name;
    ExprPtr value;

    Arg clone() const;
  };

  ExprPtr expr;
  vector<Arg> args;

  CallExpr(ExprPtr e, vector<Arg> &&a);
  /// Simple call e(arg)
  CallExpr(ExprPtr e, ExprPtr arg = nullptr, ExprPtr arg2 = nullptr,
           ExprPtr arg3 = nullptr);
  /// Simple call e(a...)
  CallExpr(ExprPtr e, vector<ExprPtr> &&a);
  CallExpr(const CallExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, CallExpr);
};

struct DotExpr : public Expr {
  ExprPtr expr;
  string member;

  DotExpr(ExprPtr e, string m);
  DotExpr(const DotExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, DotExpr);
};

struct SliceExpr : public Expr {
  /// Any of these can be nullptr to account for partial slices
  ExprPtr st, ed, step;

  SliceExpr(ExprPtr s, ExprPtr e, ExprPtr st);
  SliceExpr(const SliceExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, SliceExpr);
};

struct EllipsisExpr : public Expr {
  bool isPipeArg;

  /// Expression ..., currently used in partial calls
  EllipsisExpr(bool i = false);
  EllipsisExpr(const EllipsisExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, EllipsisExpr);
};

struct TypeOfExpr : public Expr {
  ExprPtr expr;

  TypeOfExpr(ExprPtr e);
  TypeOfExpr(const TypeOfExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, TypeOfExpr);
};

struct LambdaExpr : public Expr {
  /// Expression: lambda vars...: expr
  vector<string> vars;
  ExprPtr expr;

  LambdaExpr(vector<string> &&v, ExprPtr e);
  LambdaExpr(const LambdaExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, LambdaExpr);
};

struct YieldExpr : public Expr {
  /// Expression: (yield) (send to generator)
  YieldExpr();
  YieldExpr(const YieldExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, YieldExpr);
};

/// Post-transform AST nodes

struct StmtExpr : public Expr {
  vector<unique_ptr<Stmt>> stmts;
  ExprPtr expr;

  StmtExpr(vector<unique_ptr<Stmt>> &&s, ExprPtr e);
  StmtExpr(const StmtExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, StmtExpr);
};

struct PtrExpr : public Expr {
  ExprPtr expr;

  PtrExpr(ExprPtr e);
  PtrExpr(const PtrExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, PtrExpr);
};

struct TupleIndexExpr : Expr {
  ExprPtr expr;
  int index;

  TupleIndexExpr(ExprPtr e, int i);
  TupleIndexExpr(const TupleIndexExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, TupleIndexExpr);
};

struct InstantiateExpr : Expr {
  ExprPtr type;
  vector<ExprPtr> params;

  InstantiateExpr(ExprPtr e, vector<ExprPtr> &&i);
  InstantiateExpr(ExprPtr e, ExprPtr t);
  InstantiateExpr(const InstantiateExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, InstantiateExpr);
};

struct StackAllocExpr : Expr {
  ExprPtr typeExpr, expr;

  StackAllocExpr(ExprPtr e, ExprPtr i);
  StackAllocExpr(const StackAllocExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, StackAllocExpr);
};

struct StaticExpr : public Expr {
  /// Expression: lambda vars...: expr
  ExprPtr expr;
  set<string> captures;

  StaticExpr(ExprPtr e, set<string> &&captures);
  StaticExpr(const StaticExpr &n);

  string toString() const override;
  NODE_UTILITY(Expr, StaticExpr);
};

} // namespace ast
} // namespace seq
