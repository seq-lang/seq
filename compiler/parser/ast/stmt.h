#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/expr.h"
#include "parser/ast/pattern.h"
#include "parser/ast/visitor.h"

#define ACCEPT_VISITOR                                                         \
  virtual void accept(StmtVisitor &visitor) const override {                   \
    visitor.visit(this);                                                       \
  }

namespace seq {
namespace ast {

struct Param {
  std::string name;
  ExprPtr type;
  ExprPtr deflt;
  std::string to_string() const;
};

struct Stmt : public seq::SrcObject {
  Stmt() = default;
  Stmt(const seq::SrcInfo &s);
  virtual ~Stmt() {}
  virtual std::string to_string() const = 0;
  virtual void accept(StmtVisitor &) const = 0;
  virtual std::vector<Stmt *> getStatements();
  friend std::ostream &operator<<(std::ostream &out, const Stmt &c) {
    return out << c.to_string();
  }
};

typedef std::unique_ptr<Stmt> StmtPtr;

struct SuiteStmt : public Stmt {
  using Stmt::Stmt;
  std::vector<StmtPtr> stmts;
  SuiteStmt(std::vector<StmtPtr> s);
  std::string to_string() const override;
  std::vector<Stmt *> getStatements() override;
  ACCEPT_VISITOR;
};

struct PassStmt : public Stmt {
  PassStmt();
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct BreakStmt : public Stmt {
  BreakStmt();
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct ContinueStmt : public Stmt {
  ContinueStmt();
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct ExprStmt : public Stmt {
  ExprPtr expr;
  ExprStmt(ExprPtr e);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct AssignStmt : public Stmt {
  ExprPtr lhs, rhs, type;
  bool mustExist;
  AssignStmt(ExprPtr l, ExprPtr r, ExprPtr t = nullptr, bool m = false);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct DelStmt : public Stmt {
  ExprPtr expr;
  DelStmt(ExprPtr e);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct PrintStmt : public Stmt {
  ExprPtr expr;
  PrintStmt(ExprPtr i);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct ReturnStmt : public Stmt {
  ExprPtr expr;
  ReturnStmt(ExprPtr e);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct YieldStmt : public Stmt {
  ExprPtr expr;
  YieldStmt(ExprPtr e);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct AssertStmt : public Stmt {
  ExprPtr expr;
  AssertStmt(ExprPtr e);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct TypeAliasStmt : public Stmt {
  std::string name;
  ExprPtr expr;
  TypeAliasStmt(std::string n, ExprPtr e);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct WhileStmt : public Stmt {
  ExprPtr cond;
  StmtPtr suite;
  WhileStmt(ExprPtr c, StmtPtr s);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct ForStmt : public Stmt {
  ExprPtr var;
  ExprPtr iter;
  StmtPtr suite;
  ForStmt(ExprPtr v, ExprPtr i, StmtPtr s);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct IfStmt : public Stmt {
  struct If {
    ExprPtr cond;
    StmtPtr suite;
  };
  std::vector<If> ifs;
  IfStmt(std::vector<If> i);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct MatchStmt : public Stmt {
  ExprPtr what;
  std::vector<std::pair<PatternPtr, StmtPtr>> cases;
  MatchStmt(ExprPtr w, std::vector<std::pair<PatternPtr, StmtPtr>> c);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct ExtendStmt : public Stmt {
  ExprPtr what;
  StmtPtr suite;
  ExtendStmt(ExprPtr e, StmtPtr s);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct ImportStmt : public Stmt {
  typedef std::pair<std::string, std::string> Item;
  Item from;
  std::vector<Item> what;
  ImportStmt(Item f, std::vector<Item> w);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct ExternImportStmt : public Stmt {
  ImportStmt::Item name;
  ExprPtr from;
  ExprPtr ret;
  std::vector<Param> args;
  std::string lang;
  ExternImportStmt(ImportStmt::Item n, ExprPtr f, ExprPtr t,
                   std::vector<Param> a, std::string l);
  std::string to_string() const override;
  ACCEPT_VISITOR;
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
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct GlobalStmt : public Stmt {
  std::string var;
  GlobalStmt(std::string v);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct ThrowStmt : public Stmt {
  ExprPtr expr;
  ThrowStmt(ExprPtr e);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct PrefetchStmt : public Stmt {
  ExprPtr expr;
  PrefetchStmt(ExprPtr e);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct FunctionStmt : public Stmt {
  std::string name;
  ExprPtr ret;
  std::vector<std::string> generics;
  std::vector<Param> args;
  StmtPtr suite;
  std::vector<std::string> attributes;
  FunctionStmt(std::string n, ExprPtr r, std::vector<std::string> g,
               std::vector<Param> a, StmtPtr s, std::vector<std::string> at);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct PyDefStmt : public Stmt {
  std::string name;
  ExprPtr ret;
  std::vector<Param> args;
  std::string code;
  PyDefStmt(std::string n, ExprPtr r, std::vector<Param> a, std::string s);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct ClassStmt : public Stmt {
  bool isType;
  std::string name;
  std::vector<std::string> generics;
  std::vector<Param> args;
  StmtPtr suite;
  ClassStmt(bool i, std::string n, std::vector<std::string> g,
            std::vector<Param> a, StmtPtr s);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct DeclareStmt : public Stmt {
  Param param;
  DeclareStmt(Param p);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct AssignEqStmt : public Stmt {
  ExprPtr lhs, rhs;
  std::string op;
  AssignEqStmt(ExprPtr l, ExprPtr r, std::string o);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct YieldFromStmt : public Stmt {
  ExprPtr expr;
  YieldFromStmt(ExprPtr e);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct WithStmt : public Stmt {
  typedef std::pair<ExprPtr, std::string> Item;
  std::vector<Item> items;
  StmtPtr suite;
  WithStmt(std::vector<Item> i, StmtPtr s);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

} // namespace ast
} // namespace seq

#undef ACCEPT_VISITOR
