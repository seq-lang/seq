#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "parser/expr.h"
#include "parser/pattern.h"
#include "parser/visitor.h"
#include "seq/seq.h"

using std::ostream;
using std::string;
using std::unique_ptr;
using std::vector;
using std::pair;

struct Param {
  string name;
  ExprPtr type;
  ExprPtr deflt;
  string to_string() const;
};

struct Stmt : public seq::SrcObject {
  Stmt() = default;
  Stmt(const seq::SrcInfo &s);
  virtual ~Stmt() {}
  virtual string to_string() const = 0;
  virtual void accept(StmtVisitor &) const = 0;
  virtual vector<Stmt*> getStatements();
  friend ostream &operator<<(ostream &out, const Stmt &c) {
    return out << c.to_string();
  }
};

typedef unique_ptr<Stmt> StmtPtr;

#define ACCEPT_VISITOR                                                      \
  virtual void accept(StmtVisitor &visitor) const override { visitor.visit(this); }

struct SuiteStmt : public Stmt {
  using Stmt::Stmt;
  vector<StmtPtr> stmts;
  SuiteStmt(vector<StmtPtr> s);
  string to_string() const override;
  vector<Stmt*> getStatements() override;
  ACCEPT_VISITOR;
};

struct PassStmt : public Stmt {
  PassStmt();
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct BreakStmt : public Stmt {
  BreakStmt();
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct ContinueStmt : public Stmt {
  ContinueStmt();
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct ExprStmt : public Stmt {
  ExprPtr expr;
  ExprStmt(ExprPtr e);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct AssignStmt : public Stmt {
  ExprPtr lhs, rhs, type;
  AssignStmt(ExprPtr l, ExprPtr r, ExprPtr t = nullptr);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct DelStmt : public Stmt {
  ExprPtr expr;
  DelStmt(ExprPtr e);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct PrintStmt : public Stmt {
  ExprPtr expr;
  PrintStmt(ExprPtr i);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct ReturnStmt : public Stmt {
  ExprPtr expr;
  ReturnStmt(ExprPtr e);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct YieldStmt : public Stmt {
  ExprPtr expr;
  YieldStmt(ExprPtr e);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct AssertStmt : public Stmt {
  ExprPtr expr;
  AssertStmt(ExprPtr e);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct TypeAliasStmt : public Stmt {
  string name;
  ExprPtr expr;
  TypeAliasStmt(string n, ExprPtr e);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct WhileStmt : public Stmt {
  ExprPtr cond;
  StmtPtr suite;
  WhileStmt(ExprPtr c, StmtPtr s);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct ForStmt : public Stmt {
  ExprPtr var;
  ExprPtr iter;
  StmtPtr suite;
  ForStmt(ExprPtr v, ExprPtr i, StmtPtr s);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct IfStmt : public Stmt {
  struct If {
    ExprPtr cond;
    StmtPtr suite;
  };
  vector<If> ifs;
  IfStmt(vector<If> i);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct MatchStmt : public Stmt {
  ExprPtr what;
  vector<pair<PatternPtr, StmtPtr>> cases;
  MatchStmt(ExprPtr w, vector<pair<PatternPtr, StmtPtr>> c);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct ExtendStmt : public Stmt {
  ExprPtr what;
  StmtPtr suite;
  ExtendStmt(ExprPtr e, StmtPtr s);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct ImportStmt : public Stmt {
  typedef pair<string, string> Item;
  Item from;
  vector<Item> what;
  ImportStmt(Item f, vector<Item> w);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct ExternImportStmt : public Stmt {
  ImportStmt::Item name;
  ExprPtr from;
  ExprPtr ret;
  vector<Param> args;
  string lang;
  ExternImportStmt(ImportStmt::Item n, ExprPtr f, ExprPtr t, vector<Param> a, string l);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct TryStmt : public Stmt {
  struct Catch {
    string var;
    ExprPtr exc;
    StmtPtr suite;
  };
  StmtPtr suite;
  vector<Catch> catches;
  StmtPtr finally;

  TryStmt(StmtPtr s, vector<Catch> c, StmtPtr f);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct GlobalStmt : public Stmt {
  string var;
  GlobalStmt(string v);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct ThrowStmt : public Stmt {
  ExprPtr expr;
  ThrowStmt(ExprPtr e);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct PrefetchStmt : public Stmt {
  ExprPtr expr;
  PrefetchStmt(ExprPtr e);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct FunctionStmt : public Stmt {
  string name;
  ExprPtr ret;
  vector<string> generics;
  vector<Param> args;
  StmtPtr suite;
  vector<string> attributes;
  FunctionStmt(string n, ExprPtr r, vector<string> g, vector<Param> a, StmtPtr s, vector<string> at);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct ClassStmt : public Stmt {
  bool isType;
  string name;
  vector<string> generics;
  vector<Param> args;
  StmtPtr suite;
  ClassStmt(bool i, string n, vector<string> g, vector<Param> a, StmtPtr s);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct DeclareStmt : public Stmt {
  Param param;
  DeclareStmt(Param p);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct AssignEqStmt : public Stmt {
  ExprPtr lhs, rhs;
  string op;
  AssignEqStmt(ExprPtr l, ExprPtr r, string o);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct YieldFromStmt : public Stmt {
  ExprPtr expr;
  YieldFromStmt(ExprPtr e);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct WithStmt : public Stmt {
  typedef pair<ExprPtr, string> Item;
  vector<Item> items;
  StmtPtr suite;
  WithStmt(vector<Item> i, StmtPtr s);
  string to_string() const override;
  ACCEPT_VISITOR;
};

#undef ACCEPT_VISITOR
