#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/types.h"
#include "parser/ast/visitor.h"

#define ACCEPT_VISITOR(X)                                                      \
  virtual void accept(X##Visitor &visitor) const override {                    \
    visitor.visit(this);                                                       \
  }

namespace seq {
namespace ast {

struct Expr : public seq::SrcObject {
  std::shared_ptr<Type> type;
  bool _isType;
  Expr() : type(nullptr), _isType(false) {}
  virtual ~Expr() {}
  virtual std::string toString() const = 0;
  virtual void accept(ExprVisitor &) const = 0;

  shared_ptr<Type> getType() const { return type; }
  void setType(shared_ptr<Type> t) { type = t; }
  bool isType() const { return _isType; }
  void markType() { _isType = true; }

  friend std::ostream &operator<<(std::ostream &out, const Expr &c) {
    return out << c.toString();
  }
};

struct Stmt : public seq::SrcObject {
  Stmt() = default;
  Stmt(const seq::SrcInfo &s);
  virtual ~Stmt() {}
  virtual std::string toString() const = 0;
  virtual void accept(StmtVisitor &) const = 0;
  virtual std::vector<Stmt *> getStatements();
  friend std::ostream &operator<<(std::ostream &out, const Stmt &c) {
    return out << c.toString();
  }
};

struct Pattern : public seq::SrcObject {
  virtual ~Pattern() {}
  virtual std::string toString() const = 0;
  virtual void accept(PatternVisitor &) const = 0;
  friend std::ostream &operator<<(std::ostream &out, const Pattern &c) {
    return out << c.toString();
  }
};

typedef std::unique_ptr<Expr> ExprPtr;
typedef std::unique_ptr<Stmt> StmtPtr;
typedef std::unique_ptr<Pattern> PatternPtr;

struct Param {
  std::string name;
  ExprPtr type;
  ExprPtr deflt;
  std::string toString() const;
};

struct EmptyExpr : public Expr {
  EmptyExpr();
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
  std::string suffix;
  IntExpr(int v);
  IntExpr(std::string v, std::string s = "");
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct FloatExpr : public Expr {
  double value;
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
  bool inPlace;
  BinaryExpr(ExprPtr l, std::string o, ExprPtr r, bool i = false);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct PipeExpr : public Expr {
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
  struct Arg {
    std::string name;
    ExprPtr value;
  };
  ExprPtr expr;
  std::vector<Arg> args;
  CallExpr(ExprPtr e, std::vector<Arg> a);
  CallExpr(ExprPtr e, std::vector<ExprPtr> a);
  CallExpr(ExprPtr e, ExprPtr arg);
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
  ExprPtr st, ed, step;
  SliceExpr(ExprPtr s, ExprPtr e, ExprPtr st);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct EllipsisExpr : public Expr {
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
  std::vector<std::string> vars;
  ExprPtr expr;
  LambdaExpr(std::vector<std::string> v, ExprPtr e);
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct YieldExpr : public Expr {
  YieldExpr();
  std::string toString() const override;
  ACCEPT_VISITOR(Expr);
};

struct SuiteStmt : public Stmt {
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
  ExprPtr lhs, rhs, type;
  bool mustExist, force;
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
  ExprPtr expr;
  ReturnStmt(ExprPtr e);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct YieldStmt : public Stmt {
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

struct TypeAliasStmt : public Stmt {
  std::string name;
  ExprPtr expr;
  TypeAliasStmt(std::string n, ExprPtr e);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct WhileStmt : public Stmt {
  ExprPtr cond;
  StmtPtr suite;
  WhileStmt(ExprPtr c, StmtPtr s);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct ForStmt : public Stmt {
  ExprPtr var;
  ExprPtr iter;
  StmtPtr suite;
  ForStmt(ExprPtr v, ExprPtr i, StmtPtr s);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct IfStmt : public Stmt {
  struct If {
    ExprPtr cond;
    StmtPtr suite;
  };
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
  Item from;
  std::vector<Item> what;
  ImportStmt(Item f, std::vector<Item> w);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct ExternImportStmt : public Stmt {
  ImportStmt::Item name;
  ExprPtr from;
  ExprPtr ret;
  std::vector<Param> args;
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
  Param param;
  DeclareStmt(Param p);
  std::string toString() const override;
  ACCEPT_VISITOR(Stmt);
};

struct AssignEqStmt : public Stmt {
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
