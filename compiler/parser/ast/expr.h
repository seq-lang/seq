#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/visitor.h"

#define ACCEPT_VISITOR                                                         \
  virtual void accept(ExprVisitor &visitor) const override {                   \
    visitor.visit(this);                                                       \
  }

namespace seq {
namespace ast {

struct Expr : public seq::SrcObject {
  virtual ~Expr() {}
  virtual std::string to_string() const = 0;
  virtual void accept(ExprVisitor &) const = 0;
  friend std::ostream &operator<<(std::ostream &out, const Expr &c) {
    return out << c.to_string();
  }
};
typedef std::unique_ptr<Expr> ExprPtr;

struct EmptyExpr : public Expr {
  EmptyExpr();
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct BoolExpr : public Expr {
  bool value;
  BoolExpr(bool v);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct IntExpr : public Expr {
  std::string value;
  std::string suffix;
  IntExpr(int v);
  IntExpr(std::string v, std::string s = "");
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct FloatExpr : public Expr {
  double value;
  std::string suffix;
  FloatExpr(double v, std::string s = "");
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct StringExpr : public Expr {
  std::string value;
  StringExpr(std::string v);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct FStringExpr : public Expr {
  std::string value;
  FStringExpr(std::string v);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct KmerExpr : public Expr {
  std::string value;
  KmerExpr(std::string v);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct SeqExpr : public Expr {
  std::string prefix;
  std::string value;
  SeqExpr(std::string v, std::string p = "s");
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct IdExpr : public Expr {
  std::string value;
  IdExpr(std::string v);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct UnpackExpr : public Expr {
  ExprPtr what;
  UnpackExpr(ExprPtr w);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct TupleExpr : public Expr {
  std::vector<ExprPtr> items;
  TupleExpr(std::vector<ExprPtr> i);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct ListExpr : public Expr {
  std::vector<ExprPtr> items;
  ListExpr(std::vector<ExprPtr> i);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct SetExpr : public Expr {
  std::vector<ExprPtr> items;
  SetExpr(std::vector<ExprPtr> i);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct DictExpr : public Expr {
  struct KeyValue {
    ExprPtr key, value;
  };
  std::vector<KeyValue> items;
  DictExpr(std::vector<KeyValue> it);
  std::string to_string() const override;
  ACCEPT_VISITOR;
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
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct DictGeneratorExpr : public Expr {
  ExprPtr key, expr;
  std::vector<GeneratorExpr::Body> loops;
  DictGeneratorExpr(ExprPtr k, ExprPtr e, std::vector<GeneratorExpr::Body> l);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct IfExpr : public Expr {
  ExprPtr cond, eif, eelse;
  IfExpr(ExprPtr c, ExprPtr i, ExprPtr e);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct UnaryExpr : public Expr {
  std::string op;
  ExprPtr expr;
  UnaryExpr(std::string o, ExprPtr e);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct BinaryExpr : public Expr {
  std::string op;
  ExprPtr lexpr, rexpr;
  bool inPlace;
  BinaryExpr(ExprPtr l, std::string o, ExprPtr r, bool i = false);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct PipeExpr : public Expr {
  struct Pipe {
    std::string op;
    ExprPtr expr;
  };
  std::vector<Pipe> items;
  PipeExpr(std::vector<Pipe> it);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct IndexExpr : public Expr {
  ExprPtr expr, index;
  IndexExpr(ExprPtr e, ExprPtr i);
  std::string to_string() const override;
  ACCEPT_VISITOR;
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
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct DotExpr : public Expr {
  ExprPtr expr;
  std::string member;
  DotExpr(ExprPtr e, std::string m);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct SliceExpr : public Expr {
  ExprPtr st, ed, step;
  SliceExpr(ExprPtr s, ExprPtr e, ExprPtr st);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct EllipsisExpr : public Expr {
  EllipsisExpr();
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct TypeOfExpr : public Expr {
  ExprPtr expr;
  TypeOfExpr(ExprPtr e);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct PtrExpr : public Expr {
  ExprPtr expr;
  PtrExpr(ExprPtr e);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct LambdaExpr : public Expr {
  std::vector<std::string> vars;
  ExprPtr expr;
  LambdaExpr(std::vector<std::string> v, ExprPtr e);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct YieldExpr : public Expr {
  YieldExpr();
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

} // namespace ast
} // namespace seq

#undef ACCEPT_VISITOR
