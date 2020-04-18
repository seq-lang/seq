#include "util/fmt/format.h"
#include "util/fmt/ostream.h"
#include <memory>
#include <ostream>
#include <stack>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/ast/ast.h"
#include "parser/ast/format.h"
#include "parser/ast/transform.h"
#include "parser/common.h"
#include "parser/context.h"
#include "parser/ocaml.h"

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
  this->result = fmt::format(T, __VA_ARGS__);                                  \
  return

namespace seq {
namespace ast {

string FormatExprVisitor::transform(const Expr *expr) {
  FormatExprVisitor v;
  expr->accept(v);
  return v.result;
}

string FormatExprVisitor::transform(const vector<ExprPtr> &exprs) {
  string r;
  for (auto &e : exprs) {
    r += transform(e) + ", ";
  }
  return r;
}

void FormatExprVisitor::visit(const EmptyExpr *expr) { this->result = "None"; }

void FormatExprVisitor::visit(const BoolExpr *expr) {
  RETURN("{}", expr->value ? "True" : "False");
}

void FormatExprVisitor::visit(const IntExpr *expr) {
  RETURN("{}{}", expr->value, expr->suffix);
}

void FormatExprVisitor::visit(const FloatExpr *expr) {
  RETURN("{}{}", expr->value, expr->suffix);
}

void FormatExprVisitor::visit(const StringExpr *expr) {
  RETURN("\"{}\"", escape(expr->value));
}

void FormatExprVisitor::visit(const FStringExpr *expr) {
  RETURN("f\"{}\"", escape(expr->value));
}

void FormatExprVisitor::visit(const KmerExpr *expr) {
  RETURN("k\"{}\"", escape(expr->value));
}

void FormatExprVisitor::visit(const SeqExpr *expr) {
  RETURN("{}\"{}\"", expr->prefix, escape(expr->value));
}

void FormatExprVisitor::visit(const IdExpr *expr) { RETURN("{}", expr->value); }

void FormatExprVisitor::visit(const UnpackExpr *expr) {
  RETURN("*{}", transform(expr->what));
}

void FormatExprVisitor::visit(const TupleExpr *expr) {
  RETURN("({})", transform(expr->items));
}

void FormatExprVisitor::visit(const ListExpr *expr) {
  RETURN("[{}]", transform(expr->items));
}

void FormatExprVisitor::visit(const SetExpr *expr) {
  RETURN("{{{}}}", transform(expr->items));
}

void FormatExprVisitor::visit(const DictExpr *expr) {
  vector<string> items;
  for (auto &i : expr->items) {
    items.push_back(format("{}: {}", transform(i.key), transform(i.value)));
  }
  RETURN("{{{}}}", fmt::join(items, ", "));
}

void FormatExprVisitor::visit(const GeneratorExpr *expr) {
  string s;
  for (auto &i : expr->loops) {
    string cond;
    for (auto &k : i.conds) {
      cond += format(" if {}", transform(k));
    }
    s += format("for {} in {}{}", fmt::join(i.vars, ", "), i.gen->to_string(),
                cond);
  }
  if (expr->kind == GeneratorExpr::ListGenerator) {
    RETURN("[{} {}]", transform(expr->expr), s);
  } else if (expr->kind == GeneratorExpr::SetGenerator) {
    RETURN("{{{} {}}}", transform(expr->expr), s);
  } else {
    RETURN("({} {})", transform(expr->expr), s);
  }
}

void FormatExprVisitor::visit(const DictGeneratorExpr *expr) {
  string s;
  for (auto &i : expr->loops) {
    string cond;
    for (auto &k : i.conds) {
      cond += format(" if {}", transform(k));
    }
    s += format("for {} in {}{}", fmt::join(i.vars, ", "), i.gen->to_string(),
                cond);
  }
  RETURN("{{{}: {} {}}}", transform(expr->key), transform(expr->expr), s);
}

void FormatExprVisitor::visit(const IfExpr *expr) {
  RETURN("{} if {} else {}", transform(expr->eif), transform(expr->cond),
         transform(expr->eelse));
}

void FormatExprVisitor::visit(const UnaryExpr *expr) {
  RETURN("{}{}", expr->op, transform(expr->expr));
}

void FormatExprVisitor::visit(const BinaryExpr *expr) {
  RETURN("({} {} {})", transform(expr->lexpr), expr->op,
         transform(expr->rexpr));
}

void FormatExprVisitor::visit(const PipeExpr *expr) {
  vector<string> items;
  for (auto &l : expr->items) {
    if (!items.size()) {
      items.push_back(transform(l.expr));
    } else {
      items.push_back(l.op + " " + transform(l.expr));
    }
  }
  RETURN("({})", fmt::join(items, " "));
}

void FormatExprVisitor::visit(const IndexExpr *expr) {
  RETURN("{}[{}]", transform(expr->expr), transform(expr->index));
}

void FormatExprVisitor::visit(const CallExpr *expr) {
  vector<string> args;
  for (auto &i : expr->args) {
    if (i.name == "") {
      args.push_back(transform(i.value));
    } else {
      args.push_back(format("{}: {}", i.name, transform(i.value)));
    }
  }
  RETURN("{}({})", transform(expr->expr), fmt::join(args, ", "));
}

void FormatExprVisitor::visit(const DotExpr *expr) {
  RETURN("{}.{}", transform(expr->expr), expr->member);
}

void FormatExprVisitor::visit(const SliceExpr *expr) {
  string s;
  if (expr->st) {
    s += transform(expr->st);
  }
  s += ":";
  if (expr->ed) {
    s += transform(expr->ed);
  }
  s += ":";
  if (expr->step) {
    s += transform(expr->step);
  }
  this->result = s;
}

void FormatExprVisitor::visit(const EllipsisExpr *expr) {
  this->result = "...";
}

void FormatExprVisitor::visit(const TypeOfExpr *expr) {
  RETURN("typeof({})", transform(expr->expr));
}

void FormatExprVisitor::visit(const PtrExpr *expr) {
  RETURN("__ptr__({})", transform(expr->expr));
}

void FormatExprVisitor::visit(const LambdaExpr *expr) {
  RETURN("lambda {}: {}", fmt::join(expr->vars, ", "), transform(expr->expr));
}

void FormatExprVisitor::visit(const YieldExpr *expr) {
  this->result = "(yield)";
}

string FormatStmtVisitor::transform(const StmtPtr &stmt, int indent) {
  FormatStmtVisitor v;
  v.indent = this->indent + indent;
  stmt->accept(v);
  return v.result;
}

string FormatStmtVisitor::transform(const ExprPtr &expr) {
  return FormatExprVisitor().transform(expr.get());
}

string FormatStmtVisitor::transform(const PatternPtr &pat) {
  return FormatPatternVisitor().transform(pat.get());
}

string FormatStmtVisitor::pad(int indent) {
  return string((this->indent + indent) * 2, ' ');
}

void FormatStmtVisitor::visit(const SuiteStmt *stmt) {
  string result;
  for (auto &i : stmt->stmts) {
    result += transform(i);
  }
  this->result = result;
}

void FormatStmtVisitor::visit(const PassStmt *stmt) { this->result = "pass"; }

void FormatStmtVisitor::visit(const BreakStmt *stmt) { this->result = "break"; }

void FormatStmtVisitor::visit(const ContinueStmt *stmt) {
  this->result = "continue";
}

void FormatStmtVisitor::visit(const ExprStmt *stmt) {
  RETURN("{}", transform(stmt->expr));
}

void FormatStmtVisitor::visit(const AssignStmt *stmt) {
  if (stmt->type) {
    RETURN("{}: {} = {}", transform(stmt->lhs), transform(stmt->type),
           transform(stmt->rhs));
  } else if (stmt->mustExist) {
    RETURN("{}", transform(stmt->rhs));
  } else {
    RETURN("{} = {}", transform(stmt->lhs), transform(stmt->rhs));
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
  RETURN("while {}:\n{}", transform(stmt->cond), transform(stmt->suite, 1));
}

void FormatStmtVisitor::visit(const ForStmt *stmt) {
  RETURN("for {} in {}:\n{}", transform(stmt->var), transform(stmt->iter),
         transform(stmt->suite, 1));
}

void FormatStmtVisitor::visit(const IfStmt *stmt) {
  string ifs;
  string prefix = "";
  for (auto &ifc : stmt->ifs) {
    if (ifc.cond) {
      ifs += format("{}{}if {}:\n{}", pad(), prefix, transform(ifc.cond),
                    transform(ifc.suite, 1));
    } else {
      ifs += format("{}else:\n{}", pad(), transform(ifc.suite, 1));
    }
    prefix = "el";
  }
  this->result = ifs + "\n";
}

void FormatStmtVisitor::visit(const MatchStmt *stmt) {
  string s;
  for (auto &c : stmt->cases) {
    s += format("{}case {}:\n{}\n", pad(1), transform(c.first),
                transform(c.second, 2));
  }
  RETURN("match {}:\n{}", transform(stmt->what), s);
}

void FormatStmtVisitor::visit(const ExtendStmt *stmt) {
  RETURN("extend {}:\n{}", transform(stmt->what), transform(stmt->suite, 1));
}

void FormatStmtVisitor::visit(const ImportStmt *stmt) {
  auto fix = [](const string &s) {
    string r = s;
    for (auto &c : r) {
      if (c == '/')
        c = '.';
    }
    return r;
  };
  if (stmt->what.size() == 0) {
    RETURN("import {}{}", fix(stmt->from.first),
           stmt->from.second == "" ? "" : format(" as {}", stmt->from.second));
  } else {
    vector<string> what;
    for (auto &w : stmt->what) {
      what.push_back(format("{}{}", fix(w.first),
                            w.second == "" ? "" : format(" as {}", w.second)));
    }
    RETURN("from {} import {}", fix(stmt->from.first), fmt::join(what, ", "));
  }
}

void FormatStmtVisitor::visit(const ExternImportStmt *stmt) {
  this->result = "";
}

void FormatStmtVisitor::visit(const TryStmt *stmt) {
  vector<string> catches;
  for (auto &c : stmt->catches) {
    catches.push_back(format("catch {}{}\n:{}", transform(c.exc),
                             c.var == "" ? "" : format(" as {}", c.var),
                             transform(c.suite, 1)));
  }
  RETURN("try:\n{}{}{}", transform(stmt->suite, 1), fmt::join(catches, ""),
         stmt->finally ? format("finally:\n{}", transform(stmt->finally, 1))
                       : "");
}

void FormatStmtVisitor::visit(const GlobalStmt *stmt) {
  RETURN("global {}", stmt->var);
}

void FormatStmtVisitor::visit(const ThrowStmt *stmt) {
  RETURN("raise {}", transform(stmt->expr));
}

void FormatStmtVisitor::visit(const FunctionStmt *stmt) {
  string attrs;
  for (auto &a : stmt->attributes) {
    attrs += format("{}@{}\n", pad(), a);
  }
  vector<string> args;
  for (auto &a : stmt->args) {
    args.push_back(format("{}{}{}", a.name,
                          a.type ? format(": {}", transform(a.type)) : "",
                          a.deflt ? format(" = {}", transform(a.deflt)) : ""));
  }
  this->result = format("{}{}def {}{}({}){}:\n{}", attrs, pad(), stmt->name,
                        !stmt->generics.empty()
                            ? format("[{}]", fmt::join(stmt->generics, ", "))
                            : "",
                        fmt::join(args, ", "),
                        stmt->ret ? format(" -> {}", transform(stmt->ret)) : "",
                        transform(stmt->suite, 1));
}

void FormatStmtVisitor::visit(const ClassStmt *stmt) {
  vector<string> args;
  for (auto &a : stmt->args) {
    args.push_back(format("{}{}{}", a.name,
                          a.type ? format(": {}", transform(a.type)) : "",
                          a.deflt ? format(" = {}", transform(a.deflt)) : ""));
  }

  if (stmt->isType) {
    auto t = transform(stmt->suite, 1);
    RETURN("type {}({}){}", stmt->name, fmt::join(args, ", "),
           !t.empty() ? format(":\n{}", t) : "");

  } else {
    string as;
    for (auto &a : args) {
      as += pad(1) + a + "\n";
    }
    RETURN("class {}{}:\n{}{}", stmt->name,
           !stmt->generics.empty()
               ? format("[{}]", fmt::join(stmt->generics, ", "))
               : "",
           as, transform(stmt->suite, 1));
  }
}

void FormatStmtVisitor::visit(const DeclareStmt *stmt) {
  RETURN("{}: {}", stmt->param.name, transform(stmt->param.type));
}

void FormatStmtVisitor::visit(const AssignEqStmt *stmt) {
  RETURN("{} {}= {}", transform(stmt->lhs), stmt->op, transform(stmt->rhs));
}

void FormatStmtVisitor::visit(const YieldFromStmt *stmt) {
  RETURN("yield from {}", transform(stmt->expr));
}

void FormatStmtVisitor::visit(const WithStmt *stmt) {
  vector<string> what;
  for (auto &w : stmt->items) {
    what.push_back(format("{}{}", *w.first,
                          w.second == "" ? "" : format(" as {}", w.second)));
  }
  RETURN("with {}:\n{}", fmt::join(what, ", "), transform(stmt->suite, 1));
}

void FormatStmtVisitor::visit(const PyDefStmt *stmt) {}

string FormatPatternVisitor::transform(const Pattern *ptr) {
  FormatPatternVisitor v;
  ptr->accept(v);
  return v.result;
}

void FormatPatternVisitor::visit(const StarPattern *pat) {
  this->result = "...";
}

void FormatPatternVisitor::visit(const IntPattern *pat) {
  RETURN("{}", pat->value);
}

void FormatPatternVisitor::visit(const BoolPattern *pat) {
  RETURN("{}", pat->value ? "True" : "False");
}

void FormatPatternVisitor::visit(const StrPattern *pat) {
  RETURN("\"{}\"", escape(pat->value));
}

void FormatPatternVisitor::visit(const SeqPattern *pat) {
  RETURN("s\"{}\"", escape(pat->value));
}

void FormatPatternVisitor::visit(const RangePattern *pat) {
  RETURN("{} ... {}", pat->start, pat->end);
}

void FormatPatternVisitor::visit(const TuplePattern *pat) {
  string r;
  for (auto &e : pat->patterns) {
    r += transform(e) + ", ";
  }
  RETURN("({})", r);
}

void FormatPatternVisitor::visit(const ListPattern *pat) {
  string r;
  for (auto &e : pat->patterns) {
    r += transform(e) + ", ";
  }
  RETURN("[{}}]", r);
}

void FormatPatternVisitor::visit(const OrPattern *pat) {
  vector<string> r;
  for (auto &e : pat->patterns) {
    r.push_back(format("({})", transform(e)));
  }
  RETURN("{}", fmt::join(r, " or "));
}

void FormatPatternVisitor::visit(const WildcardPattern *pat) {
  RETURN("{}", pat->var == "" ? "_" : pat->var);
}

void FormatPatternVisitor::visit(const GuardedPattern *pat) {
  RETURN("{} if {}", transform(pat->pattern),
         FormatExprVisitor().transform(pat->cond));
}

void FormatPatternVisitor::visit(const BoundPattern *pat) {
  RETURN("({}) as {}", transform(pat->pattern), pat->var);
}

} // namespace ast
} // namespace seq
