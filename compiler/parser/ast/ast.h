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
#include "parser/ast/types.h"
#include "parser/ast/visitor.h"

/// Macro that makes node accept XVisitor
#define ACCEPT_VISITOR(X)                                                      \
  virtual void accept(X##Visitor &visitor) const override {                    \
    visitor.visit(this);                                                       \
  }

namespace seq {
namespace ast {

struct Expr : public seq::SrcObject {
private:
  /// Each expression comes with an associated type.
  /// Types are nullptr until realized by a typechecker.
  TypePtr _type;

  /// Flag that indicates is this expression a type expression
  /// (e.g. int, list[int], or generic T)
  bool _isType;

public:
  Expr() : _type(nullptr), _isType(false) {}
  virtual ~Expr() {}

  /// Convert node to a string
  virtual std::string toString() const = 0;
  /// Accept an AST walker/visitor
  virtual void accept(ExprVisitor &) const = 0;

  /// Type utilities
  TypePtr getType() const { return _type; }
  void setType(TypePtr t) { _type = t; }
  bool isType() const { return _isType; }
  void markType() { _isType = true; }

  /// Allow pretty-printing to C++ streams
  friend std::ostream &operator<<(std::ostream &out, const Expr &c) {
    return out << c.toString();
  }
};

struct Stmt : public seq::SrcObject {
  Stmt() = default;
  Stmt(const seq::SrcInfo &s);
  virtual ~Stmt() {}

  /// Convert node to a string
  virtual std::string toString() const = 0;
  /// Accept an AST walker/visitor
  virtual void accept(StmtVisitor &) const = 0;

  /// Get child statements (e.g. block contents).
  /// Returns the statement itself if it has no child statements.
  virtual std::vector<Stmt *> getStatements();

  /// Allow pretty-printing to C++ streams
  friend std::ostream &operator<<(std::ostream &out, const Stmt &c) {
    return out << c.toString();
  }
};

struct Pattern : public seq::SrcObject {
  virtual ~Pattern() {}

  /// Convert node to a string
  virtual std::string toString() const = 0;
  /// Accept an AST walker/visitor
  virtual void accept(PatternVisitor &) const = 0;

  /// Allow pretty-printing to C++ streams
  friend std::ostream &operator<<(std::ostream &out, const Pattern &c) {
    return out << c.toString();
  }
};

typedef std::unique_ptr<Expr> ExprPtr;
typedef std::unique_ptr<Stmt> StmtPtr;
typedef std::unique_ptr<Pattern> PatternPtr;

/// Type that models the function parameters
/// (name: type = deflt)
struct Param {
  std::string name;
  ExprPtr type;
  ExprPtr deflt;
  std::string toString() const;
};

struct NoneExpr : public Expr {
  NoneExpr();
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct BoolExpr : public Expr {
  bool value;

  BoolExpr(bool v);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct IntExpr : public Expr {
  std::string value;
  /// Number suffix (e.g. "u" for "123u")
  std::string suffix;

  IntExpr(int v);
  IntExpr(std::string v, std::string s = "");
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct FloatExpr : public Expr {
  double value;
  /// Number suffix (e.g. "u" for "123u")
  std::string suffix;

  FloatExpr(double v, std::string s = "");
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct StringExpr : public Expr {
  std::string value;

  StringExpr(std::string v);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct FStringExpr : public Expr {
  std::string value;

  FStringExpr(std::string v);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct KmerExpr : public Expr {
  std::string value;

  KmerExpr(std::string v);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct SeqExpr : public Expr {
  /// Sequence prefix (e.g. "p" for "p'AU'")
  std::string prefix;
  std::string value;

  SeqExpr(std::string v, std::string p = "s");
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct IdExpr : public Expr {
  std::string value;

  IdExpr(std::string v);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct UnpackExpr : public Expr {
  /// Unpack expression: (*what)
  ExprPtr what;

  UnpackExpr(ExprPtr w);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct TupleExpr : public Expr {
  std::vector<ExprPtr> items;

  TupleExpr(std::vector<ExprPtr> i);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct ListExpr : public Expr {
  std::vector<ExprPtr> items;

  ListExpr(std::vector<ExprPtr> i);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct SetExpr : public Expr {
  std::vector<ExprPtr> items;

  SetExpr(std::vector<ExprPtr> i);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct DictExpr : public Expr {
  struct KeyValue {
    ExprPtr key, value;
  };
  std::vector<KeyValue> items;

  DictExpr(std::vector<KeyValue> it);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct GeneratorExpr : public Expr {
  /// Generator expression: [expr (loops...)]
  /// where loops are: for vars... in gen (if conds...)?
  enum Kind { Generator, ListGenerator, SetGenerator };
  struct Body {
    std::vector<std::string> vars;
    ExprPtr gen;
    std::vector<ExprPtr> conds;
  };

  Kind kind;
  ExprPtr expr;
  std::vector<Body> loops;

  GeneratorExpr(Kind k, ExprPtr e, std::vector<Body> l);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct DictGeneratorExpr : public Expr {
  /// Dictionary generator expression: {key: expr (loops...)}
  /// where loops are: for vars... in gen (if conds...)?

  ExprPtr key, expr;
  std::vector<GeneratorExpr::Body> loops;

  DictGeneratorExpr(ExprPtr k, ExprPtr e, std::vector<GeneratorExpr::Body> l);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct IfExpr : public Expr {
  ExprPtr cond, eif, eelse;

  IfExpr(ExprPtr c, ExprPtr i, ExprPtr e);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct UnaryExpr : public Expr {
  std::string op;
  ExprPtr expr;

  UnaryExpr(std::string o, ExprPtr e);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct BinaryExpr : public Expr {
  std::string op;
  ExprPtr lexpr, rexpr;
  /// Does this expression modify lhs (e.g. a += b)?
  bool inPlace;

  BinaryExpr(ExprPtr l, std::string o, ExprPtr r, bool i = false);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct PipeExpr : public Expr {
  /// Pipe expression: [op expr]...
  /// The first item has op = ""; others have op = "|>" or op = "||>"

  struct Pipe {
    std::string op;
    ExprPtr expr;
  };

  std::vector<Pipe> items;

  PipeExpr(std::vector<Pipe> it);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct IndexExpr : public Expr {
  ExprPtr expr, index;

  IndexExpr(ExprPtr e, ExprPtr i);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct CallExpr : public Expr {
  /// Each argument can have a name (e.g. foo(1, b=5))
  struct Arg {
    std::string name;
    ExprPtr value;
  };

  ExprPtr expr;
  std::vector<Arg> args;

  CallExpr(ExprPtr e, std::vector<Arg> a);
  /// Simple call e(a...)
  CallExpr(ExprPtr e, std::vector<ExprPtr> a);
  /// Simple call e(arg)
  CallExpr(ExprPtr e, ExprPtr arg);
  /// Simple call e()
  CallExpr(ExprPtr e);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct DotExpr : public Expr {
  ExprPtr expr;
  std::string member;

  DotExpr(ExprPtr e, std::string m);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct SliceExpr : public Expr {
  /// Any of these can be nullptr to account for partial slices
  ExprPtr st, ed, step;

  SliceExpr(ExprPtr s, ExprPtr e, ExprPtr st);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct EllipsisExpr : public Expr {
  /// Expression ..., currently used in partial calls
  EllipsisExpr();

  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct TypeOfExpr : public Expr {
  ExprPtr expr;

  TypeOfExpr(ExprPtr e);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct PtrExpr : public Expr {
  ExprPtr expr;

  PtrExpr(ExprPtr e);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct LambdaExpr : public Expr {
  /// Expression: lambda vars...: expr
  std::vector<std::string> vars;
  ExprPtr expr;

  LambdaExpr(std::vector<std::string> v, ExprPtr e);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct YieldExpr : public Expr {
  /// Expression: (yield) (send to generator)
  YieldExpr();

  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct SuiteStmt : public Stmt {
  /// Represents list (block) of statements.
  /// Does not necessarily imply new variable block.
  using Stmt::Stmt;

  std::vector<StmtPtr> stmts;

  SuiteStmt(std::vector<StmtPtr> s);
  std::string toString() const override;
  std::vector<Stmt *> getStatements() override;
  ACCEPT_VISITOR(Stmt);
};

struct PassStmt : public Stmt {
  PassStmt();
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct BreakStmt : public Stmt {
  BreakStmt();
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct ContinueStmt : public Stmt {
  ContinueStmt();
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct ExprStmt : public Stmt {
  ExprPtr expr;

  ExprStmt(ExprPtr e);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct AssignStmt : public Stmt {
  /// Statement: lhs : type = rhs
  ExprPtr lhs, rhs, type;
  /// mustExist indicates that lhs must exist (e.g. a += b).
  bool mustExist;
  /// force controls if lhs will shadow existing lhs or not.
  bool force;

  AssignStmt(ExprPtr l, ExprPtr r, ExprPtr t = nullptr, bool m = false,
             bool f = false);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct DelStmt : public Stmt {
  ExprPtr expr;

  DelStmt(ExprPtr e);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct PrintStmt : public Stmt {
  ExprPtr expr;

  PrintStmt(ExprPtr i);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct ReturnStmt : public Stmt {
  /// Might be nullptr for empty return/yield statements
  ExprPtr expr;

  ReturnStmt(ExprPtr e);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct YieldStmt : public Stmt {
  /// Might be nullptr for empty return/yield statements
  ExprPtr expr;

  YieldStmt(ExprPtr e);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct AssertStmt : public Stmt {
  ExprPtr expr;

  AssertStmt(ExprPtr e);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct WhileStmt : public Stmt {
  /// Statement: while cond: suite
  ExprPtr cond;
  StmtPtr suite;

  WhileStmt(ExprPtr c, StmtPtr s);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct ForStmt : public Stmt {
  /// Statement: for var in iter: suite
  ExprPtr var;
  ExprPtr iter;
  StmtPtr suite;

  ForStmt(ExprPtr v, ExprPtr i, StmtPtr s);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct IfStmt : public Stmt {
  /// Statement: if cond: suite;
  struct If {
    ExprPtr cond;
    StmtPtr suite;
  };

  /// Last member of ifs has cond = nullptr (else block)
  std::vector<If> ifs;

  IfStmt(std::vector<If> i);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct MatchStmt : public Stmt {
  ExprPtr what;
  std::vector<std::pair<PatternPtr, StmtPtr>> cases;

  MatchStmt(ExprPtr w, std::vector<std::pair<PatternPtr, StmtPtr>> c);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct ExtendStmt : public Stmt {
  ExprPtr what;
  StmtPtr suite;

  ExtendStmt(ExprPtr e, StmtPtr s);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct ImportStmt : public Stmt {
  typedef std::pair<std::string, std::string> Item;

  /// Statement:
  /// 1. from from[0] import what...
  /// 2. import from[0] as from[1]
  /// where what is: what[0] as what[1]
  Item from;
  std::vector<Item> what;

  ImportStmt(Item f, std::vector<Item> w);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct ExternImportStmt : public Stmt {
  ImportStmt::Item name;

  ExprPtr from;
  /// Return type for foreign imports
  ExprPtr ret;
  /// Argument types for foreign import
  std::vector<Param> args;
  /// Indicates language (c, py, r)
  std::string lang;

  ExternImportStmt(ImportStmt::Item n, ExprPtr f, ExprPtr t,
                   std::vector<Param> a, std::string l);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct TryStmt : public Stmt {
  struct Catch {
    std::string var;
    ExprPtr exc;
    StmtPtr suite;
  };

  StmtPtr suite;
  std::vector<Catch> catches;
  StmtPtr finally;

  TryStmt(StmtPtr s, std::vector<Catch> c, StmtPtr f);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct GlobalStmt : public Stmt {
  std::string var;

  GlobalStmt(std::string v);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct ThrowStmt : public Stmt {
  ExprPtr expr;

  ThrowStmt(ExprPtr e);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct FunctionStmt : public Stmt {
  std::string name;
  ExprPtr ret;
  std::vector<std::string> generics;
  std::vector<Param> args;
  std::shared_ptr<Stmt> suite;
  /// List of attributes (e.g. @internal @prefetch)
  std::vector<std::string> attributes;

  FunctionStmt(std::string n, ExprPtr r, std::vector<std::string> g,
               std::vector<Param> a, std::shared_ptr<Stmt> s,
               std::vector<std::string> at);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct PyDefStmt : public Stmt {
  std::string name;
  ExprPtr ret;
  std::vector<Param> args;
  std::string code;

  PyDefStmt(std::string n, ExprPtr r, std::vector<Param> a, std::string s);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct ClassStmt : public Stmt {
  /// Is it type (record) or a class?
  bool isRecord;
  std::string name;
  std::vector<std::string> generics;
  std::vector<Param> args;
  StmtPtr suite;

  ClassStmt(bool i, std::string n, std::vector<std::string> g,
            std::vector<Param> a, StmtPtr s);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct DeclareStmt : public Stmt {
  /// Used only within ClassStmt to model a dataclass-like class
  Param param;

  DeclareStmt(Param p);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct AssignEqStmt : public Stmt {
  /// Statement: lhs op= rhs
  ExprPtr lhs, rhs;
  std::string op;

  AssignEqStmt(ExprPtr l, ExprPtr r, std::string o);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct YieldFromStmt : public Stmt {
  ExprPtr expr;

  YieldFromStmt(ExprPtr e);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct WithStmt : public Stmt {
  typedef std::pair<ExprPtr, std::string> Item;
  std::vector<Item> items;
  StmtPtr suite;

  WithStmt(std::vector<Item> i, StmtPtr s);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct StarPattern : public Pattern {
  StarPattern();
  std::string toString() const override;
  ACCEPT_VISITOR(Pattern);
};

struct IntPattern : public Pattern {
  int value;

  IntPattern(int v);
  std::string toString() const override;
  ACCEPT_VISITOR(Pattern);
};

struct BoolPattern : public Pattern {
  bool value;

  BoolPattern(bool v);
  std::string toString() const override;
  ACCEPT_VISITOR(Pattern);
};

struct StrPattern : public Pattern {
  std::string value;

  StrPattern(std::string v);
  std::string toString() const override;
  ACCEPT_VISITOR(Pattern);
};

struct SeqPattern : public Pattern {
  std::string value;

  SeqPattern(std::string v);
  std::string toString() const override;
  ACCEPT_VISITOR(Pattern);
};

struct RangePattern : public Pattern {
  int start, end;

  RangePattern(int s, int e);
  std::string toString() const override;
  ACCEPT_VISITOR(Pattern);
};

struct TuplePattern : public Pattern {
  std::vector<PatternPtr> patterns;

  TuplePattern(std::vector<PatternPtr> p);
  std::string toString() const override;
  ACCEPT_VISITOR(Pattern);
};

struct ListPattern : public Pattern {
  std::vector<PatternPtr> patterns;

  ListPattern(std::vector<PatternPtr> p);
  std::string toString() const override;
  ACCEPT_VISITOR(Pattern);
};

struct OrPattern : public Pattern {
  std::vector<PatternPtr> patterns;

  OrPattern(std::vector<PatternPtr> p);
  std::string toString() const override;
  ACCEPT_VISITOR(Pattern);
};

struct WildcardPattern : public Pattern {
  std::string var;

  WildcardPattern(std::string v);
  std::string toString() const override;
  ACCEPT_VISITOR(Pattern);
};

struct GuardedPattern : public Pattern {
  PatternPtr pattern;
  ExprPtr cond;

  GuardedPattern(PatternPtr p, ExprPtr c);
  std::string toString() const override;
  ACCEPT_VISITOR(Pattern);
};

struct BoundPattern : public Pattern {
  std::string var;
  PatternPtr pattern;

  BoundPattern(std::string v, PatternPtr p);
  std::string toString() const override;
  ACCEPT_VISITOR(Pattern);
};

} // namespace ast
} // namespace seq
