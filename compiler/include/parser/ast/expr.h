#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "parser/ast/visitor.h"
#include "seq/seq.h"

using std::ostream;
using std::string;
using std::unique_ptr;
using std::vector;

#define ACCEPT_VISITOR                                                         \
  virtual void accept(ExprVisitor &visitor) const override {                   \
    visitor.visit(this);                                                       \
  }

struct Expr : public seq::SrcObject {
  virtual ~Expr() {}
  virtual string to_string() const = 0;
  virtual void accept(ExprVisitor &) const = 0;
  friend ostream &operator<<(ostream &out, const Expr &c) {
    return out << c.to_string();
  }
};

typedef unique_ptr<Expr> ExprPtr;

struct EmptyExpr : public Expr {
  EmptyExpr();
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct BoolExpr : public Expr {
  bool value;
  BoolExpr(bool v);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct IntExpr : public Expr {
  string value;
  string suffix;
  IntExpr(int v);
  IntExpr(string v, string s = "");
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct FloatExpr : public Expr {
  double value;
  string suffix;
  FloatExpr(double v, string s = "");
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct StringExpr : public Expr {
  string value;
  StringExpr(string v);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct FStringExpr : public Expr {
  string value;
  FStringExpr(string v);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct KmerExpr : public Expr {
  string value;
  KmerExpr(string v);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct SeqExpr : public Expr {
  string prefix;
  string value;
  SeqExpr(string v, string p = "s");
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct IdExpr : public Expr {
  string value;
  IdExpr(string v);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct UnpackExpr : public Expr {
  ExprPtr what;
  UnpackExpr(ExprPtr w);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct TupleExpr : public Expr {
  vector<ExprPtr> items;
  TupleExpr(vector<ExprPtr> i);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct ListExpr : public Expr {
  vector<ExprPtr> items;
  ListExpr(vector<ExprPtr> i);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct SetExpr : public Expr {
  vector<ExprPtr> items;
  SetExpr(vector<ExprPtr> i);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct DictExpr : public Expr {
  struct KeyValue {
    ExprPtr key, value;
  };
  vector<KeyValue> items;
  DictExpr(vector<KeyValue> it);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct GeneratorExpr : public Expr {
  enum Kind { Generator, ListGenerator, SetGenerator };
  struct Body {
    vector<string> vars;
    ExprPtr gen;
    vector<ExprPtr> conds;
  };
  Kind kind;
  ExprPtr expr;
  vector<Body> loops;
  GeneratorExpr(Kind k, ExprPtr e, vector<Body> l);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct DictGeneratorExpr : public Expr {
  ExprPtr key, expr;
  vector<GeneratorExpr::Body> loops;
  DictGeneratorExpr(ExprPtr k, ExprPtr e, vector<GeneratorExpr::Body> l);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct IfExpr : public Expr {
  ExprPtr cond, eif, eelse;
  IfExpr(ExprPtr c, ExprPtr i, ExprPtr e);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct UnaryExpr : public Expr {
  string op;
  ExprPtr expr;
  UnaryExpr(string o, ExprPtr e);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct BinaryExpr : public Expr {
  string op;
  ExprPtr lexpr, rexpr;
  bool inPlace;
  BinaryExpr(ExprPtr l, string o, ExprPtr r, bool i = false);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct PipeExpr : public Expr {
  struct Pipe {
    string op;
    ExprPtr expr;
  };
  vector<Pipe> items;
  PipeExpr(vector<Pipe> it);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct IndexExpr : public Expr {
  ExprPtr expr, index;
  IndexExpr(ExprPtr e, ExprPtr i);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct CallExpr : public Expr {
  struct Arg {
    string name;
    ExprPtr value;
  };
  ExprPtr expr;
  vector<Arg> args;
  CallExpr(ExprPtr e, vector<Arg> a);
  CallExpr(ExprPtr e, vector<ExprPtr> a);
  CallExpr(ExprPtr e, ExprPtr arg);
  CallExpr(ExprPtr e);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct DotExpr : public Expr {
  ExprPtr expr;
  string member;
  DotExpr(ExprPtr e, string m);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct SliceExpr : public Expr {
  ExprPtr st, ed, step;
  SliceExpr(ExprPtr s, ExprPtr e, ExprPtr st);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct EllipsisExpr : public Expr {
  EllipsisExpr();
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct TypeOfExpr : public Expr {
  ExprPtr expr;
  TypeOfExpr(ExprPtr e);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct PtrExpr : public Expr {
  ExprPtr expr;
  PtrExpr(ExprPtr e);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct LambdaExpr : public Expr {
  vector<string> vars;
  ExprPtr expr;
  LambdaExpr(vector<string> v, ExprPtr e);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct YieldExpr : public Expr {
  YieldExpr();
  string to_string() const override;
  ACCEPT_VISITOR;
};

#undef ACCEPT_VISITOR
