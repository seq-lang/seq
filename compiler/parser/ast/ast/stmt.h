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
#include "parser/ast/ast/expr.h"
#include "parser/ast/ast/pattern.h"
#include "parser/ast/ast/visitor.h"
#include "parser/ast/types.h"
#include "parser/common.h"

namespace seq {
namespace ast {

struct Stmt : public seq::SrcObject {
  Stmt() = default;
  Stmt(const Stmt &s);
  Stmt(const seq::SrcInfo &s);
  virtual ~Stmt();
  virtual unique_ptr<Stmt> clone() const = 0;

  /// Convert node to a string
  virtual string toString() const = 0;
  /// Accept an AST walker/visitor
  virtual void accept(ASTVisitor &) const = 0;

  /// Allow pretty-printing to C++ streams
  friend std::ostream &operator<<(std::ostream &out, const Stmt &c) {
    return out << c.toString();
  }
};
typedef unique_ptr<Stmt> StmtPtr;

struct SuiteStmt : public Stmt {
  /// Represents list (block) of statements.
  /// Does not necessarily imply new variable block.
  using Stmt::Stmt;

  vector<StmtPtr> stmts;
  bool ownBlock;

  SuiteStmt(vector<StmtPtr> &&s, bool o = false);
  SuiteStmt(StmtPtr s, bool o = false);
  SuiteStmt(StmtPtr s, StmtPtr s2, bool o = false);
  SuiteStmt(const SuiteStmt &s);

  string toString() const override;
  NODE_UTILITY(Stmt, SuiteStmt);
};

struct PassStmt : public Stmt {
  PassStmt();
  PassStmt(const PassStmt &s);

  string toString() const override;
  NODE_UTILITY(Stmt, PassStmt);
};

struct BreakStmt : public Stmt {
  BreakStmt();
  BreakStmt(const BreakStmt &s);

  string toString() const override;
  NODE_UTILITY(Stmt, BreakStmt);
};

struct ContinueStmt : public Stmt {
  ContinueStmt();
  ContinueStmt(const ContinueStmt &s);

  string toString() const override;
  NODE_UTILITY(Stmt, ContinueStmt);
};

struct ExprStmt : public Stmt {
  ExprPtr expr;

  ExprStmt(ExprPtr e);
  ExprStmt(const ExprStmt &s);

  string toString() const override;
  NODE_UTILITY(Stmt, ExprStmt);
};

struct AssignStmt : public Stmt {
  /// Statement: lhs : type = rhs
  ExprPtr lhs, rhs, type;
  /// mustExist indicates that lhs must exist (e.g. a += b).
  bool mustExist;
  /// force controls if lhs will shadow existing lhs or not.
  bool force;

  AssignStmt(ExprPtr l, ExprPtr r, ExprPtr t = nullptr, bool m = false, bool f = false);
  AssignStmt(const AssignStmt &s);

  string toString() const override;
  NODE_UTILITY(Stmt, AssignStmt);
};

struct AssignEqStmt : public Stmt {
  /// Statement: lhs op= rhs
  ExprPtr lhs, rhs;
  string op;

  AssignEqStmt(ExprPtr l, ExprPtr r, const string &o);
  AssignEqStmt(const AssignEqStmt &s);

  string toString() const override;
  NODE_UTILITY(Stmt, AssignEqStmt);
};

struct DelStmt : public Stmt {
  ExprPtr expr;

  DelStmt(ExprPtr e);
  DelStmt(const DelStmt &s);

  string toString() const override;
  NODE_UTILITY(Stmt, DelStmt);
};

struct PrintStmt : public Stmt {
  ExprPtr expr;

  PrintStmt(ExprPtr i);
  PrintStmt(const PrintStmt &s);

  string toString() const override;
  NODE_UTILITY(Stmt, PrintStmt);
};

struct ReturnStmt : public Stmt {
  /// Might be nullptr for empty return/yield statements
  ExprPtr expr;

  ReturnStmt(ExprPtr e);
  ReturnStmt(const ReturnStmt &s);

  string toString() const override;
  NODE_UTILITY(Stmt, ReturnStmt);
};

struct YieldStmt : public Stmt {
  /// Might be nullptr for empty return/yield statements
  ExprPtr expr;

  YieldStmt(ExprPtr e);
  YieldStmt(const YieldStmt &s);

  string toString() const override;
  NODE_UTILITY(Stmt, YieldStmt);
};

struct AssertStmt : public Stmt {
  ExprPtr expr;

  AssertStmt(ExprPtr e);
  AssertStmt(const AssertStmt &s);

  string toString() const override;
  NODE_UTILITY(Stmt, AssertStmt);
};

struct WhileStmt : public Stmt {
  /// Statement: while cond: suite
  ExprPtr cond;
  StmtPtr suite;
  StmtPtr elseSuite;

  WhileStmt(ExprPtr c, StmtPtr s, StmtPtr e);
  WhileStmt(const WhileStmt &s);

  string toString() const override;
  NODE_UTILITY(Stmt, WhileStmt);
};

struct ForStmt : public Stmt {
  /// Statement: for var in iter: suite
  ExprPtr var;
  ExprPtr iter;
  StmtPtr suite;
  StmtPtr elseSuite;

  ForStmt(ExprPtr v, ExprPtr i, StmtPtr s, StmtPtr e = nullptr);
  ForStmt(const ForStmt &s);

  string toString() const override;
  NODE_UTILITY(Stmt, ForStmt);
};

struct IfStmt : public Stmt {
  /// Statement: if cond: suite;
  struct If {
    ExprPtr cond;
    StmtPtr suite;

    If clone() const;
  };

  /// Last member of ifs has cond = nullptr (else block)
  vector<If> ifs;

  IfStmt(vector<If> &&i);
  IfStmt(ExprPtr cond, StmtPtr suite);
  IfStmt(ExprPtr cond, StmtPtr suite, ExprPtr econd, StmtPtr esuite);
  IfStmt(const IfStmt &s);

  string toString() const override;
  NODE_UTILITY(Stmt, IfStmt);
};

struct MatchStmt : public Stmt {
  ExprPtr what;
  vector<PatternPtr> patterns;
  vector<StmtPtr> cases;

  MatchStmt(ExprPtr w, vector<PatternPtr> &&p, vector<StmtPtr> &&c);
  MatchStmt(ExprPtr w, vector<pair<PatternPtr, StmtPtr>> &&v);
  MatchStmt(const MatchStmt &s);

  string toString() const override;
  NODE_UTILITY(Stmt, MatchStmt);
};

struct ImportStmt : public Stmt {
  ExprPtr from, what;
  string as;

  ImportStmt(ExprPtr f, ExprPtr w, string a = "");
  ImportStmt(const ImportStmt &s);

  string toString() const override;
  NODE_UTILITY(Stmt, ImportStmt);
};

struct TryStmt : public Stmt {
  struct Catch {
    string var;
    ExprPtr exc;
    StmtPtr suite;

    Catch clone() const;
  };

  StmtPtr suite;
  vector<Catch> catches;
  StmtPtr finally;

  TryStmt(StmtPtr s, vector<Catch> &&c, StmtPtr f);
  TryStmt(const TryStmt &s);

  string toString() const override;
  NODE_UTILITY(Stmt, TryStmt);
};

struct GlobalStmt : public Stmt {
  string var;

  GlobalStmt(string v);
  GlobalStmt(const GlobalStmt &s);

  string toString() const override;
  NODE_UTILITY(Stmt, GlobalStmt);
};

struct ThrowStmt : public Stmt {
  ExprPtr expr;

  ThrowStmt(ExprPtr e);
  ThrowStmt(const ThrowStmt &s);

  string toString() const override;
  NODE_UTILITY(Stmt, ThrowStmt);
};

struct FunctionStmt : public Stmt {
  string name;
  ExprPtr ret;
  vector<Param> generics;
  vector<Param> args;
  StmtPtr suite;
  /// List of attributes (e.g. @internal @prefetch)
  map<string, string> attributes;

  FunctionStmt(string n, ExprPtr r, vector<Param> &&g, vector<Param> &&a, StmtPtr s,
               vector<string> &&at);
  FunctionStmt(string n, ExprPtr r, vector<Param> &&g, vector<Param> &&a, StmtPtr s,
               map<string, string> &&at);
  FunctionStmt(const FunctionStmt &s);

  string signature() const;

  string toString() const override;
  NODE_UTILITY(Stmt, FunctionStmt);
};

struct ClassStmt : public Stmt {
  /// Is it type (record) or a class?
  bool isRecord;
  string name;
  vector<Param> generics;
  vector<Param> args;
  StmtPtr suite;
  map<string, string> attributes;

  ClassStmt(bool i, string n, vector<Param> &&g, vector<Param> &&a, StmtPtr s,
            vector<string> &&at);
  ClassStmt(bool i, string n, vector<Param> &&g, vector<Param> &&a, StmtPtr s,
            map<string, string> &&at);
  ClassStmt(const ClassStmt &s);

  string toString() const override;
  NODE_UTILITY(Stmt, ClassStmt);
};

struct YieldFromStmt : public Stmt {
  ExprPtr expr;

  YieldFromStmt(ExprPtr e);
  YieldFromStmt(const YieldFromStmt &s);

  string toString() const override;
  NODE_UTILITY(Stmt, YieldFromStmt);
};

struct WithStmt : public Stmt {
  vector<ExprPtr> items;
  vector<string> vars;
  StmtPtr suite;

  WithStmt(vector<ExprPtr> &&i, vector<string> &&v, StmtPtr s);
  WithStmt(vector<pair<ExprPtr, string>> &&v, StmtPtr s);
  WithStmt(const WithStmt &s);

  string toString() const override;
  NODE_UTILITY(Stmt, WithStmt);
};

/// Post-transform AST nodes

struct AssignMemberStmt : Stmt {
  ExprPtr lhs;
  string member;
  ExprPtr rhs;

  AssignMemberStmt(ExprPtr l, string m, ExprPtr r);
  AssignMemberStmt(const AssignMemberStmt &s);

  string toString() const override;
  NODE_UTILITY(Stmt, AssignMemberStmt);
};

struct UpdateStmt : public Stmt {
  ExprPtr lhs, rhs;

  UpdateStmt(ExprPtr l, ExprPtr r);
  UpdateStmt(const UpdateStmt &s);

  string toString() const override;
  NODE_UTILITY(Stmt, UpdateStmt);
};

} // namespace ast
} // namespace seq
