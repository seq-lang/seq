#include <fmt/format.h>
#include <fmt/ostream.h>
#include <memory>
#include <ostream>
#include <stack>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/ast/expr.h"
#include "parser/ast/stmt.h"
#include "parser/ast/transform/expr.h"
#include "parser/ast/transform/pattern.h"
#include "parser/ast/transform/stmt.h"
#include "parser/ast/visitor.h"
#include "parser/common.h"
#include "parser/context.h"
#include "seq/seq.h"

using fmt::format;
using std::get;
using std::make_unique;
using std::move;
using std::ostream;
using std::stack;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::unordered_set;
using std::vector;

#define RETURN(T, ...)                                                         \
  this->result = fmt::format(T"\n", __VA_ARGS__);                                  \
  return

string FormatStmtVisitor::transform(const Stmt *stmt) {
  FormatStmtVisitor v;
  v.indent = indent;
  stmt->accept(v);
  return v.result;
}

ExprPtr FormatStmtVisitor::transform(const Expr *expr) {
  return FormatExprVisitor().transform(expr);
}

PatternPtr FormatStmtVisitor::transform(const Pattern *pat) {
  return FormatPatternVisitor().transform(expr);
}

void FormatStmtVisitor::visit(const SuiteStmt *stmt) {
  vector<StmtPtr> result;
  for (auto &i : stmt->stmts) {
    if (auto s = transform(i)) {
      result.push_back(move(s));
    }
  }
  RETURN(SuiteStmt, move(result));
}

void FormatStmtVisitor::visit(const PassStmt *stmt) { this->result = "pass\n"; }

void FormatStmtVisitor::visit(const BreakStmt *stmt) { this->result = "break\n"; }

void FormatStmtVisitor::visit(const ContinueStmt *stmt) {
  this->result = "continue\n";
}

void FormatStmtVisitor::visit(const ExprStmt *stmt) {
  RETURN("{}", transform(stmt->expr));
}

void FormatStmtVisitor::visit(const AssignStmt *stmt) {
  if (stmt->type) {
    RETURN("{}: {} = {}", transform(stmt->lexpr), transform(stmt->type), transform(stmt->rexpr));
  } else if (stmt->mustExist) {
    RETURN("{}", transform(stmt->rexpr));
  } else {
    RETURN("{} = {}", transform(stmt->lexpr), transform(stmt->rexpr));
  }
}

void FormatStmtVisitor::visit(const DelStmt *stmt) {
  RETURN("del {}", transform(stmt->expr));
}

void FormatStmtVisitor::visit(const PrintStmt *stmt) {
  RETURN("print {}", transform(stmt->expr));
}

void FormatStmtVisitor::visit(const ReturnStmt *stmt) {
  RETURN("return{}", stmt->expr ? " " + transform(stmt->expr) : "");
}

void FormatStmtVisitor::visit(const YieldStmt *stmt) {
  RETURN("yield{}", stmt->expr ? " " + transform(stmt->expr) : "");
}

void FormatStmtVisitor::visit(const AssertStmt *stmt) {
  RETURN("assert {}", transform(stmt->expr));
}

void FormatStmtVisitor::visit(const TypeAliasStmt *stmt) {
  RETURN("type {} = {}", stmt->name, transform(stmt->expr));
}

void FormatStmtVisitor::visit(const WhileStmt *stmt) {
  RETURN("while {}:\n{}", transform(stmt->cond), transform(stmt->suite, true));
}

void FormatStmtVisitor::visit(const ForStmt *stmt) {
  RETURN("for {} in {}:\n{}", transform(stmt->var), transform(stmt->iter), transform(stmt->suite, true));
}

void FormatStmtVisitor::visit(const IfStmt *stmt) {
  string ifs;
  string prefix = "";
  for (auto &ifc : stmt->ifs) {
    if (ifc.cond) {
      ifs += format("{}if {}:\n{}\n", prefix, transform(ifc.cond), transform(ifc.suite, true));
    } else {
      ifs += format("else:\n{}\n", transform(ifc.suite, true));
    }
    ifs.push_back({});
    prefix = "el";
  }
  this->result = ifs;
}

void FormatStmtVisitor::visit(const MatchStmt *stmt) {
  string s;
  for (auto &c : stmt->cases) {
    s += format("case {}:\n{}\n", transform(c.first), transform(c.second, true));
  }
  this->result = format("match {}:\n{}", transform(stmt->expr), s);
}

void FormatStmtVisitor::visit(const ExtendStmt *stmt) {
  this->result = format("extend {}:\n{}", transform(stmt->what), transform(stmt->suite, true));
}

void FormatStmtVisitor::visit(const ImportStmt *stmt) {
  // if (from.first) {

  // }
  // RETURN(ImportStmt, stmt->from, stmt->what);
}

void FormatStmtVisitor::visit(const ExternImportStmt *stmt) {

}

void FormatStmtVisitor::visit(const TryStmt *stmt) {
  vector<TryStmt::Catch> catches;
  for (auto &c : stmt->catches) {
    catches.push_back({c.var, transform(c.exc), transform(c.suite)});
  }
  RETURN(TryStmt, transform(stmt->suite), move(catches),
         transform(stmt->finally));
}

void FormatStmtVisitor::visit(const GlobalStmt *stmt) {
  RETURN(GlobalStmt, stmt->var);
}

void FormatStmtVisitor::visit(const ThrowStmt *stmt) {
  RETURN(ThrowStmt, transform(stmt->expr));
}

void FormatStmtVisitor::visit(const PrefetchStmt *stmt) {
  RETURN(PrefetchStmt, transform(stmt->expr));
}

void FormatStmtVisitor::visit(const FunctionStmt *stmt) {
  vector<Param> args;
  for (auto &a : stmt->args) {
    args.push_back({a.name, transform(a.type), transform(a.deflt)});
  }
  RETURN(FunctionStmt, stmt->name, transform(stmt->ret), stmt->generics,
         move(args), transform(stmt->suite), stmt->attributes);
}

void FormatStmtVisitor::visit(const ClassStmt *stmt) {
  vector<Param> args;
  for (auto &a : stmt->args) {
    args.push_back({a.name, transform(a.type), transform(a.deflt)});
  }
  auto suite = make_unique<SuiteStmt>(stmt->getSrcInfo());
  for (auto s : stmt->suite->getStatements()) {
    if (dynamic_cast<FunctionStmt *>(s)) {
      suite->stmts.push_back(transform(s));
    } else {
      error(s->getSrcInfo(), "types can only contain functions");
    }
  }
  RETURN(ClassStmt, stmt->isType, stmt->name, stmt->generics, move(args),
         move(suite));
}

void FormatStmtVisitor::visit(const DeclareStmt *stmt) {
  RETURN(DeclareStmt, Param{stmt->param.name, transform(stmt->param.type),
                            transform(stmt->param.deflt)});
}

void FormatStmtVisitor::visit(const AssignEqStmt *stmt) {
  RETURN(AssignStmt, transform(stmt->lhs),
         EPX(stmt, BinaryExpr, transform(stmt->lhs), stmt->op,
             transform(stmt->rhs), true),
         nullptr, true);
}

void FormatStmtVisitor::visit(const YieldFromStmt *stmt) {
  auto var = getTemporaryVar("yield");
  vector<StmtPtr> stmts;
  stmts.push_back(SP(YieldStmt, EPX(stmt, IdExpr, var)));
  RETURN(ForStmt, EPX(stmt, IdExpr, var), transform(stmt->expr),
         SP(SuiteStmt, move(stmts)));
}

void FormatStmtVisitor::visit(const WithStmt *stmt) {
  if (!stmt->items.size()) {
    ERROR("malformed with statement");
  }
  vector<StmtPtr> content;
  for (int i = stmt->items.size() - 1; i >= 0; i--) {
    vector<StmtPtr> internals;
    string var = stmt->items[i].second == "" ? getTemporaryVar("with")
                                             : stmt->items[i].second;
    internals.push_back(SP(AssignStmt, EPX(stmt, IdExpr, var),
                           transform(stmt->items[i].first)));
    internals.push_back(
        SP(ExprStmt,
           EPX(stmt, CallExpr,
               EPX(stmt, DotExpr, EPX(stmt, IdExpr, var), "__enter__"))));
    internals.push_back(
        SP(TryStmt,
           content.size() ? transform(SP(SuiteStmt, move(content)))
                          : transform(stmt->suite),
           vector<TryStmt::Catch>(),
           transform(SP(ExprStmt, EPX(stmt, CallExpr,
                                      EPX(stmt, DotExpr, EPX(stmt, IdExpr, var),
                                          "__exit__"))))));
    content = move(internals);
  }
  vector<IfStmt::If> ifs;
  ifs.push_back({EPX(stmt, BoolExpr, true), SP(SuiteStmt, move(content))});
  RETURN(IfStmt, move(ifs));
}
/*
with a as b, c as d: ...

->

if True:
  b = a
  b.__enter__()
  try:
    d = c
    c.__enter__()
    try:
      ...
    finally:
      c.__exit__()
  finally:
    b.__exit__()
*/
// void FormatStmtVisitor::visit(const PyStmt *stmt) {
//   RETURN(DeclareStmt, Param{stmt->param.name, transform(stmt->param.type),
//                             transform(stmt->param.deflt)});
// }
// pydef
/*  (* let str = Util.ppl ~sep:"\n" s ~f:(Ast.Stmt.to_string ~pythonic:true
   ~indent:1) in let p = $7 in
      (* py.exec ("""def foo(): [ind] ... """) *)
      (* from __main__ pyimport foo () -> ret *)
      let v = p, String
        (sprintf "def %s(%s):\n%s\n"
          (snd name)
          (Util.ppl fn_args ~f:(fun (_, { name; _ }) -> name))
          str) in
      let s = p, Call (
        (p, Id "_py_exec"),
        [p, { name = None; value = v }]) in
      let typ = opt_val typ ~default:($5, Id "pyobj") in
      let s' = p, ImportExtern
        [ { lang = "py"
          ; e_name = { name = snd name; typ = Some typ; default = None }
          ; e_args = []
          ; e_as = None
          ; e_from = Some (p, Id "__main__") } ]
      in
      [ p, Expr s; s' ] *) */