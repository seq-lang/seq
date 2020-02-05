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

#include "parser/ast/codegen/stmt.h"
#include "parser/ast/expr.h"
#include "parser/ast/format/expr.h"
#include "parser/ast/format/pattern.h"
#include "parser/ast/format/stmt.h"
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
using std::move;
using std::ostream;
using std::stack;
using std::string;
using std::vector;

#define RETURN0(T) this->result = fmt::format("{}" T "\n", pad());

#define RETURN(T, ...)                                                         \
  this->result = fmt::format("{}" T "\n", pad(), __VA_ARGS__);

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

void FormatStmtVisitor::visit(const PassStmt *stmt) { RETURN0("pass"); }

void FormatStmtVisitor::visit(const BreakStmt *stmt) { RETURN0("break"); }

void FormatStmtVisitor::visit(const ContinueStmt *stmt) { RETURN0("continue"); }

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

void FormatStmtVisitor::visit(const PrefetchStmt *stmt) {
  RETURN("prefetch {}", transform(stmt->expr));
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
    what.push_back(format("{}{}", w.first,
                          w.second == "" ? "" : format(" as {}", w.second)));
  }
  RETURN("with {}:\n{}", fmt::join(what, ", "), transform(stmt->suite, 1));
}

void FormatStmtVisitor::visit(const PyDefStmt *stmt) {}
