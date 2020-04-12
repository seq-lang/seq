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

#define NEWLINE "<hr>\n"
#define TYPE(t, s, ...)                                                        \
  (format("<expr><type>{}</type><node>" s,                                     \
          t->getType() ? t->getType()->str() : "", __VA_ARGS__) +              \
   format("</node></expr>"))
#define KEYWORD(x) "<b>" x " </b>"

namespace seq {
namespace ast {

FormatExprVisitor::FormatExprVisitor(TypeContext &ctx) : ctx(ctx) {}

string FormatExprVisitor::transform(const Expr *expr) {
  FormatExprVisitor v(ctx);
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

void FormatExprVisitor::visit(const NoneExpr *expr) {
  result = format("<ruby>None<rt>{}</rt></ruby>",
                  expr->getType() ? expr->getType()->str() : "-");
}

void FormatExprVisitor::visit(const BoolExpr *expr) {
  result = TYPE(expr, "{}", expr->value ? "True" : "False");
}

void FormatExprVisitor::visit(const IntExpr *expr) {
  result = TYPE(expr, "{}{}", expr->value, expr->suffix);
}

void FormatExprVisitor::visit(const FloatExpr *expr) {
  result = TYPE(expr, "{}{}", expr->value, expr->suffix);
}

void FormatExprVisitor::visit(const StringExpr *expr) {
  result = TYPE(expr, "\"{}\"", escape(expr->value));
}

void FormatExprVisitor::visit(const FStringExpr *expr) {
  result = TYPE(expr, "f\"{}\"", escape(expr->value));
}

void FormatExprVisitor::visit(const KmerExpr *expr) {
  result = TYPE(expr, "k\"{}\"", escape(expr->value));
}

void FormatExprVisitor::visit(const SeqExpr *expr) {
  result = TYPE(expr, "{}\"{}\"", expr->prefix, escape(expr->value));
}

void FormatExprVisitor::visit(const IdExpr *expr) {
  result = TYPE(expr, "{}", expr->value);
}

void FormatExprVisitor::visit(const UnpackExpr *expr) {
  result = TYPE(expr, "*{}", transform(expr->what));
}

void FormatExprVisitor::visit(const TupleExpr *expr) {
  result = TYPE(expr, "({})", transform(expr->items));
}

void FormatExprVisitor::visit(const ListExpr *expr) {
  result = TYPE(expr, "[{}]", transform(expr->items));
}

void FormatExprVisitor::visit(const SetExpr *expr) {
  result = TYPE(expr, "{{{}}}", transform(expr->items));
}

void FormatExprVisitor::visit(const DictExpr *expr) {
  vector<string> items;
  for (auto &i : expr->items) {
    items.push_back(format("{}: {}", transform(i.key), transform(i.value)));
  }
  result = TYPE(expr, "{{{}}}", fmt::join(items, ", "));
}

void FormatExprVisitor::visit(const GeneratorExpr *expr) {
  string s;
  for (auto &i : expr->loops) {
    string cond;
    for (auto &k : i.conds) {
      cond += format(" if {}", transform(k));
    }
    s += format("for {} in {}{}", fmt::join(i.vars, ", "), i.gen->toString(),
                cond);
  }
  if (expr->kind == GeneratorExpr::ListGenerator) {
    result = TYPE(expr, "[{} {}]", transform(expr->expr), s);
  } else if (expr->kind == GeneratorExpr::SetGenerator) {
    result = TYPE(expr, "{{{} {}}}", transform(expr->expr), s);
  } else {
    result = TYPE(expr, "({} {})", transform(expr->expr), s);
  }
}

void FormatExprVisitor::visit(const DictGeneratorExpr *expr) {
  string s;
  for (auto &i : expr->loops) {
    string cond;
    for (auto &k : i.conds) {
      cond += format(" if {}", transform(k));
    }
    s += format("for {} in {}{}", fmt::join(i.vars, ", "), i.gen->toString(),
                cond);
  }
  result = TYPE(expr, "{{{}: {} {}}}", transform(expr->key),
                transform(expr->expr), s);
}

void FormatExprVisitor::visit(const IfExpr *expr) {
  result = TYPE(expr, "{} if {} else {}", transform(expr->eif),
                transform(expr->cond), transform(expr->eelse));
}

void FormatExprVisitor::visit(const UnaryExpr *expr) {
  result = TYPE(expr, "{}{}", expr->op, transform(expr->expr));
}

void FormatExprVisitor::visit(const BinaryExpr *expr) {
  result = TYPE(expr, "({} {} {})", transform(expr->lexpr), expr->op,
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
  result = TYPE(expr, "({})", fmt::join(items, " "));
}

void FormatExprVisitor::visit(const IndexExpr *expr) {
  result = TYPE(expr, "{}[{}]", transform(expr->expr), transform(expr->index));
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
  result = TYPE(expr, "{}({})", transform(expr->expr), fmt::join(args, ", "));
}

void FormatExprVisitor::visit(const DotExpr *expr) {
  result = TYPE(expr, "{}.{}", transform(expr->expr), expr->member);
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
  result = TYPE(expr, "{}", s);
}

void FormatExprVisitor::visit(const EllipsisExpr *expr) {
  result = format("<ruby>None<rt>-</rt></ruby>");
}

void FormatExprVisitor::visit(const TypeOfExpr *expr) {
  result = TYPE(expr, "typeof({})", transform(expr->expr));
}

void FormatExprVisitor::visit(const PtrExpr *expr) {
  result = TYPE(expr, "__ptr__({})", transform(expr->expr));
}

void FormatExprVisitor::visit(const LambdaExpr *expr) {
  result = TYPE(expr, "lambda {}: {}", fmt::join(expr->vars, ", "),
                transform(expr->expr));
}

void FormatExprVisitor::visit(const YieldExpr *expr) {
  result = format("<ruby><b>(yield)</b><rt>{}</rt></ruby>",
                  expr->getType() ? expr->getType()->str() : "-");
}

FormatStmtVisitor::FormatStmtVisitor(TypeContext &ctx) : ctx(ctx) {}

string FormatStmtVisitor::transform(const Stmt *stmt, int indent) {
  FormatStmtVisitor v(ctx);
  v.indent = this->indent + indent;
  if (stmt)
    stmt->accept(v);
  return v.result;
}

string FormatStmtVisitor::transform(const StmtPtr &stmt, int indent) {
  return transform(stmt.get(), indent);
}

string FormatStmtVisitor::transform(const ExprPtr &expr) {
  return FormatExprVisitor(ctx).transform(expr.get());
}

string FormatStmtVisitor::transform(const PatternPtr &pat) {
  return FormatPatternVisitor().transform(pat.get());
}

string FormatStmtVisitor::pad(int indent) {
  string s;
  for (int i = 0; i < (this->indent + indent) * 2; i++)
    s += "&nbsp;";
  return s;
}

void FormatStmtVisitor::visit(const SuiteStmt *stmt) {
  string result;
  for (auto &i : stmt->stmts) {
    result += pad() + transform(i) + NEWLINE;
  }
  this->result = result;
}

void FormatStmtVisitor::visit(const PassStmt *stmt) {
  result = KEYWORD("pass");
}

void FormatStmtVisitor::visit(const BreakStmt *stmt) {
  result = KEYWORD("break");
}

void FormatStmtVisitor::visit(const ContinueStmt *stmt) {
  result = KEYWORD("continue");
}

void FormatStmtVisitor::visit(const ExprStmt *stmt) {
  result = transform(stmt->expr);
}

void FormatStmtVisitor::visit(const AssignStmt *stmt) {
  if (stmt->type) {
    result = format("{}: {} = {}", transform(stmt->lhs), transform(stmt->type),
                    transform(stmt->rhs));
  } else if (stmt->mustExist) {
    result = format("{}", transform(stmt->rhs));
  } else {
    result = format("{} = {}", transform(stmt->lhs), transform(stmt->rhs));
  }
}

void FormatStmtVisitor::visit(const DelStmt *stmt) {
  result = format(KEYWORD("del") " {}", transform(stmt->expr));
}

void FormatStmtVisitor::visit(const PrintStmt *stmt) {
  result = format(KEYWORD("print") " {}", transform(stmt->expr));
}

void FormatStmtVisitor::visit(const ReturnStmt *stmt) {
  result = format(KEYWORD("return") "{}",
                  stmt->expr ? " " + transform(stmt->expr) : "");
}

void FormatStmtVisitor::visit(const YieldStmt *stmt) {
  result = format(KEYWORD("yield") "{}",
                  stmt->expr ? " " + transform(stmt->expr) : "");
}

void FormatStmtVisitor::visit(const AssertStmt *stmt) {
  result = format(KEYWORD("assert") " {}", transform(stmt->expr));
}

// void FormatStmtVisitor::visit(const TypeAliasStmt *stmt) {
//   result =
//       format(KEYWORD("type") " {} = {}", stmt->name, transform(stmt->expr));
// }

void FormatStmtVisitor::visit(const WhileStmt *stmt) {
  result = format(KEYWORD("while") " {}:" NEWLINE "{}", transform(stmt->cond),
                  transform(stmt->suite, 1));
}

void FormatStmtVisitor::visit(const ForStmt *stmt) {
  result = format(KEYWORD("for") " {} " KEYWORD("in") " {}:" NEWLINE "{}",
                  transform(stmt->var), transform(stmt->iter),
                  transform(stmt->suite, 1));
}

void FormatStmtVisitor::visit(const IfStmt *stmt) {
  string ifs;
  string prefix = "";
  for (auto &ifc : stmt->ifs) {
    if (ifc.cond) {
      ifs += format("{}{}" KEYWORD("if") " {}:" NEWLINE "{}", pad(),
                    prefix == "" ? prefix : "<b>" + prefix + "</b>",
                    transform(ifc.cond), transform(ifc.suite, 1));
    } else {
      ifs += format("{}" KEYWORD("else") ":" NEWLINE "{}", pad(),
                    transform(ifc.suite, 1));
    }
    prefix = "el";
  }
  this->result = ifs + NEWLINE;
}

void FormatStmtVisitor::visit(const MatchStmt *stmt) {
  string s;
  for (auto &c : stmt->cases) {
    s += format("{}" KEYWORD("case") " {}:" NEWLINE "{}" NEWLINE, pad(1),
                transform(c.first), transform(c.second, 2));
  }
  result =
      format(KEYWORD("match") " {}:" NEWLINE "{}", transform(stmt->what), s);
}

void FormatStmtVisitor::visit(const ExtendStmt *stmt) {
  result = format(KEYWORD("extend") " {}:" NEWLINE "{}", transform(stmt->what),
                  transform(stmt->suite, 1));
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
    result = format(KEYWORD("import") " {}{}", fix(stmt->from.first),
                    stmt->from.second == ""
                        ? ""
                        : format(" " KEYWORD("as") " {}", stmt->from.second));
  } else {
    vector<string> what;
    for (auto &w : stmt->what) {
      what.push_back(format(
          "{}{}", fix(w.first),
          w.second == "" ? "" : format(" " KEYWORD("as") " {}", w.second)));
    }
    result = format(KEYWORD("from") " {} " KEYWORD("import") " {}",
                    fix(stmt->from.first), fmt::join(what, ", "));
  }
}

void FormatStmtVisitor::visit(const ExternImportStmt *stmt) {
  this->result = "";
}

void FormatStmtVisitor::visit(const TryStmt *stmt) {
  vector<string> catches;
  for (auto &c : stmt->catches) {
    catches.push_back(
        format(KEYWORD("catch") " {}{}" NEWLINE ":{}", transform(c.exc),
               c.var == "" ? "" : format(" " KEYWORD("as") " {}", c.var),
               transform(c.suite, 1)));
  }
  result = format(KEYWORD("try") ":" NEWLINE "{}{}{}",
                  transform(stmt->suite, 1), fmt::join(catches, ""),
                  stmt->finally ? format(KEYWORD("finally") ":" NEWLINE "{}",
                                         transform(stmt->finally, 1))
                                : "");
}

void FormatStmtVisitor::visit(const GlobalStmt *stmt) {
  result = format(KEYWORD("global") " {}", stmt->var);
}

void FormatStmtVisitor::visit(const ThrowStmt *stmt) {
  result = format(KEYWORD("raise") " {}", transform(stmt->expr));
}

void FormatStmtVisitor::visit(const FunctionStmt *stmt) {
  auto cn = ctx.getCanonicalName(stmt->getSrcInfo());
  result +=
      format("{}<b class=comment> # DEF-FUN {} CANONICAL {} TYPE {}</b><hr>\n",
             pad(), stmt->name, cn, ctx.funcASTs[cn].first->str());
  for (auto &i : ctx.getRealizations(stmt)) {
    auto *fstmt = dynamic_cast<const FunctionStmt *>(i.second);
    assert(fstmt);
    string attrs;
    for (auto &a : fstmt->attributes) {
      attrs += format("{}@{}" NEWLINE, pad(), a);
    }
    vector<string> args;
    for (auto &a : fstmt->args) {
      args.push_back(format(
          "{}{}{}", a.name, a.type ? format(": {}", transform(a.type)) : "",
          a.deflt ? format(" = {}", transform(a.deflt)) : ""));
    }
    result +=
        format("{}{}" KEYWORD("def") " {}.{}{}({}){}:" NEWLINE "{}" NEWLINE,
               attrs, pad(), ctx.getCanonicalName(fstmt->getSrcInfo()), i.first,
               !fstmt->generics.empty()
                   ? format("[{}]", fmt::join(fstmt->generics, ", "))
                   : "",
               fmt::join(args, ", "),
               fstmt->ret ? format(" -> {}", transform(fstmt->ret)) : "",
               transform(fstmt->suite.get(), 1));
  }
}

void FormatStmtVisitor::visit(const ClassStmt *stmt) {
  vector<string> args;
  for (auto &a : stmt->args) {
    args.push_back(format("{}{}{}", a.name,
                          a.type ? format(": {}", transform(a.type)) : "",
                          a.deflt ? format(" = {}", transform(a.deflt)) : ""));
  }

  if (stmt->isRecord) {
    auto t = transform(stmt->suite, 1);
    result =
        format(KEYWORD("type") " {}({}){}", stmt->name, fmt::join(args, ", "),
               !t.empty() ? format(":" NEWLINE "{}", t) : "");

  } else {
    string as;
    for (auto &a : args) {
      as += pad(1) + a + NEWLINE;
    }
    result = format(KEYWORD("class") " {}{}:" NEWLINE "{}{}", stmt->name,
                    !stmt->generics.empty()
                        ? format("[{}]", fmt::join(stmt->generics, ", "))
                        : "",
                    as, transform(stmt->suite, 1));
  }
}

void FormatStmtVisitor::visit(const DeclareStmt *stmt) {
  result = format("{}: {}", stmt->param.name, transform(stmt->param.type));
}

void FormatStmtVisitor::visit(const AssignEqStmt *stmt) {
  result =
      format("{} {}= {}", transform(stmt->lhs), stmt->op, transform(stmt->rhs));
}

void FormatStmtVisitor::visit(const YieldFromStmt *stmt) {
  result = format(KEYWORD("yield from") " {}", transform(stmt->expr));
}

void FormatStmtVisitor::visit(const WithStmt *stmt) {
  vector<string> what;
  for (auto &w : stmt->items) {
    what.push_back(format("{}{}", *w.first,
                          w.second == "" ? "" : format(" as {}", w.second)));
  }
  result = format(KEYWORD("with") " {}:" NEWLINE "{}", fmt::join(what, ", "),
                  transform(stmt->suite, 1));
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
  result = format("{}", pat->value);
}

void FormatPatternVisitor::visit(const BoolPattern *pat) {
  result = format("{}", pat->value ? "True" : "False");
}

void FormatPatternVisitor::visit(const StrPattern *pat) {
  result = format("\"{}\"", escape(pat->value));
}

void FormatPatternVisitor::visit(const SeqPattern *pat) {
  result = format("s\"{}\"", escape(pat->value));
}

void FormatPatternVisitor::visit(const RangePattern *pat) {
  result = format("{} ... {}", pat->start, pat->end);
}

void FormatPatternVisitor::visit(const TuplePattern *pat) {
  string r;
  for (auto &e : pat->patterns) {
    r += transform(e) + ", ";
  }
  result = format("({})", r);
}

void FormatPatternVisitor::visit(const ListPattern *pat) {
  string r;
  for (auto &e : pat->patterns) {
    r += transform(e) + ", ";
  }
  result = format("[{}}]", r);
}

void FormatPatternVisitor::visit(const OrPattern *pat) {
  vector<string> r;
  for (auto &e : pat->patterns) {
    r.push_back(format("({})", transform(e)));
  }
  result = format("{}", fmt::join(r, " or "));
}

void FormatPatternVisitor::visit(const WildcardPattern *pat) {
  result = format("{}", pat->var == "" ? "_" : pat->var);
}

void FormatPatternVisitor::visit(const GuardedPattern *pat) {
  // TODO
  // result = format("{} if {}", transform(pat->pattern),
  //                FormatExprVisitor(ctx).transform(pat->cond));
}

void FormatPatternVisitor::visit(const BoundPattern *pat) {
  result = format("({}) as {}", transform(pat->pattern), pat->var);
}

} // namespace ast
} // namespace seq
