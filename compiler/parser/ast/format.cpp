#include <ostream>
#include <string>
#include <vector>

#include "parser/ast/format.h"

using fmt::format;
using std::string;
using std::vector;

namespace seq {
namespace ast {

FormatVisitor::FormatVisitor(TypeContext &ctx)
    : ctx(ctx), space("&nbsp;"), renderType(true), indent(0) {}

string FormatVisitor::transform(const ExprPtr &expr) {
  FormatVisitor v(ctx);
  if (expr)
    expr->accept(v);
  return v.result;
}

string FormatVisitor::transform(const StmtPtr &stmt, int indent) {
  FormatVisitor v(ctx);
  v.indent = this->indent + indent;
  if (stmt)
    stmt->accept(v);
  return v.result;
}

string FormatVisitor::transform(const PatternPtr &ptr) {
  FormatVisitor v(ctx);
  if (ptr)
    ptr->accept(v);
  return v.result;
}

string FormatVisitor::pad(int indent) const {
  string s;
  for (int i = 0; i < (this->indent + indent) * 2; i++)
    s += space;
  return s;
}

string FormatVisitor::newline() const { return "<hr>\n"; }

string FormatVisitor::keyword(const string &s) const {
  return format("<b>{}</b>", s);
}

/*************************************************************************************/

void FormatVisitor::visit(const NoneExpr *expr) {
  result = renderExpr(expr, "None");
}

void FormatVisitor::visit(const BoolExpr *expr) {
  result = renderExpr(expr, "{}", expr->value ? "True" : "False");
}

void FormatVisitor::visit(const IntExpr *expr) {
  result = renderExpr(expr, "{}{}", expr->value, expr->suffix);
}

void FormatVisitor::visit(const FloatExpr *expr) {
  result = renderExpr(expr, "{}{}", expr->value, expr->suffix);
}

void FormatVisitor::visit(const StringExpr *expr) {
  result = renderExpr(expr, "\"{}\"", escape(expr->value));
}

void FormatVisitor::visit(const FStringExpr *expr) {
  result = renderExpr(expr, "f\"{}\"", escape(expr->value));
}

void FormatVisitor::visit(const KmerExpr *expr) {
  result = renderExpr(expr, "k\"{}\"", escape(expr->value));
}

void FormatVisitor::visit(const SeqExpr *expr) {
  result = renderExpr(expr, "{}\"{}\"", expr->prefix, escape(expr->value));
}

void FormatVisitor::visit(const IdExpr *expr) {
  result = renderExpr(expr, "{}", expr->value);
}

void FormatVisitor::visit(const UnpackExpr *expr) {
  result = renderExpr(expr, "*{}", transform(expr->what));
}

void FormatVisitor::visit(const TupleExpr *expr) {
  result = renderExpr(expr, "({})", transform(expr->items));
}

void FormatVisitor::visit(const ListExpr *expr) {
  result = renderExpr(expr, "[{}]", transform(expr->items));
}

void FormatVisitor::visit(const SetExpr *expr) {
  result = renderExpr(expr, "{{{}}}", transform(expr->items));
}

void FormatVisitor::visit(const DictExpr *expr) {
  vector<string> s;
  for (auto &i : expr->items)
    s.push_back(format("{}: {}", transform(i.key), transform(i.value)));
  result = renderExpr(expr, "{{{}}}", join(s, ", "));
}

void FormatVisitor::visit(const GeneratorExpr *expr) {
  string s;
  for (auto &i : expr->loops) {
    string cond;
    for (auto &k : i.conds)
      cond += format(" if {}", transform(k));
    s += format("for {} in {}{}", fmt::join(i.vars, ", "), i.gen->toString(),
                cond);
  }
  if (expr->kind == GeneratorExpr::ListGenerator)
    result = renderExpr(expr, "[{} {}]", transform(expr->expr), s);
  else if (expr->kind == GeneratorExpr::SetGenerator)
    result = renderExpr(expr, "{{{} {}}}", transform(expr->expr), s);
  else
    result = renderExpr(expr, "({} {})", transform(expr->expr), s);
}

void FormatVisitor::visit(const DictGeneratorExpr *expr) {
  string s;
  for (auto &i : expr->loops) {
    string cond;
    for (auto &k : i.conds)
      cond += format(" if {}", transform(k));

    s += format("for {} in {}{}", fmt::join(i.vars, ", "), i.gen->toString(),
                cond);
  }
  result = renderExpr(expr, "{{{}: {} {}}}", transform(expr->key),
                      transform(expr->expr), s);
}

void FormatVisitor::visit(const IfExpr *expr) {
  result = renderExpr(expr, "{} if {} else {}", transform(expr->eif),
                      transform(expr->cond), transform(expr->eelse));
}

void FormatVisitor::visit(const UnaryExpr *expr) {
  result = renderExpr(expr, "{}{}", expr->op, transform(expr->expr));
}

void FormatVisitor::visit(const BinaryExpr *expr) {
  result = renderExpr(expr, "({} {} {})", transform(expr->lexpr), expr->op,
                      transform(expr->rexpr));
}

void FormatVisitor::visit(const PipeExpr *expr) {
  vector<string> items;
  for (auto &l : expr->items) {
    if (!items.size())
      items.push_back(transform(l.expr));
    else
      items.push_back(l.op + " " + transform(l.expr));
  }
  result = renderExpr(expr, "({})", join(items, " "));
}

void FormatVisitor::visit(const IndexExpr *expr) {
  result =
      renderExpr(expr, "{}[{}]", transform(expr->expr), transform(expr->index));
}

void FormatVisitor::visit(const CallExpr *expr) {
  vector<string> args;
  for (auto &i : expr->args) {
    if (i.name == "")
      args.push_back(transform(i.value));
    else
      args.push_back(format("{}: {}", i.name, transform(i.value)));
  }
  result = renderExpr(expr, "{}({})", transform(expr->expr), join(args, ", "));
}

void FormatVisitor::visit(const DotExpr *expr) {
  result = renderExpr(expr, "{}.{}", transform(expr->expr), expr->member);
}

void FormatVisitor::visit(const SliceExpr *expr) {
  string s;
  if (expr->st)
    s += transform(expr->st);
  s += ":";
  if (expr->ed)
    s += transform(expr->ed);
  s += ":";
  if (expr->step)
    s += transform(expr->step);
  result = renderExpr(expr, "{}", s);
}

void FormatVisitor::visit(const EllipsisExpr *expr) {
  result = renderExpr(expr, "...");
}

void FormatVisitor::visit(const TypeOfExpr *expr) {
  result = renderExpr(expr, "{}({})", keyword("typeof"), transform(expr->expr));
}

void FormatVisitor::visit(const PtrExpr *expr) {
  result = renderExpr(expr, "__ptr__({})", transform(expr->expr));
}

void FormatVisitor::visit(const LambdaExpr *expr) {
  result = renderExpr(expr, "{} {}: {}", keyword("lambda"),
                      join(expr->vars, ", "), transform(expr->expr));
}

void FormatVisitor::visit(const YieldExpr *expr) {
  result = renderExpr(expr, "(yield)");
}

void FormatVisitor::visit(const SuiteStmt *stmt) {
  string result;
  for (auto &i : stmt->stmts) {
    result += fmt::format("{}{}{}", pad(), transform(i), newline());
  }
  this->result = result;
}

void FormatVisitor::visit(const PassStmt *stmt) { result = keyword("pass"); }

void FormatVisitor::visit(const BreakStmt *stmt) { result = keyword("break"); }

void FormatVisitor::visit(const ContinueStmt *stmt) {
  result = keyword("continue");
}

void FormatVisitor::visit(const ExprStmt *stmt) {
  result = transform(stmt->expr);
}

void FormatVisitor::visit(const AssignStmt *stmt) {
  if (stmt->type) {
    result = format("{}: {} = {}", transform(stmt->lhs), transform(stmt->type),
                    transform(stmt->rhs));
  } else if (stmt->mustExist) {
    result = format("{}", transform(stmt->rhs));
  } else {
    result = format("{} = {}", transform(stmt->lhs), transform(stmt->rhs));
  }
}

void FormatVisitor::visit(const DelStmt *stmt) {
  result = format("{} {}", keyword("del"), transform(stmt->expr));
}

void FormatVisitor::visit(const PrintStmt *stmt) {
  result = format("{} {}", keyword("print"), transform(stmt->expr));
}

void FormatVisitor::visit(const ReturnStmt *stmt) {
  result = format("{}{}", keyword("return"),
                  stmt->expr ? " " + transform(stmt->expr) : "");
}

void FormatVisitor::visit(const YieldStmt *stmt) {
  result = format("{}{}", keyword("yield"),
                  stmt->expr ? " " + transform(stmt->expr) : "");
}

void FormatVisitor::visit(const AssertStmt *stmt) {
  result = format("{} {}", keyword("assert"), transform(stmt->expr));
}

void FormatVisitor::visit(const WhileStmt *stmt) {
  result = format("{} {}:{}{}", keyword("while"), transform(stmt->cond),
                  newline(), transform(stmt->suite, 1));
}

void FormatVisitor::visit(const ForStmt *stmt) {
  result = format("{} {} {} {}:{}{}", keyword("for"), transform(stmt->var),
                  keyword("in"), transform(stmt->iter), newline(),
                  transform(stmt->suite, 1));
}

void FormatVisitor::visit(const IfStmt *stmt) {
  string ifs;
  string prefix = "";
  for (auto &ifc : stmt->ifs) {
    if (ifc.cond)
      ifs += format("{}{} {}:{}{}", pad(), keyword(prefix + "if"),
                    transform(ifc.cond), newline(), transform(ifc.suite, 1));
    else
      ifs += format("{}{}:{}{}", pad(), keyword("else"), newline(),
                    transform(ifc.suite, 1));
    prefix = "el";
  }
  result = ifs + newline();
}

void FormatVisitor::visit(const MatchStmt *stmt) {
  string s;
  for (int ci = 0; ci < stmt->cases.size(); ci++)
    s += format("{}{}{}:{}{}{}", pad(1), keyword("case"),
                transform(stmt->patterns[ci]), newline(),
                transform(stmt->cases[ci], 2), newline());
  result = format("{} {}:{}{}", keyword("match"), transform(stmt->what),
                  newline(), s);
}

void FormatVisitor::visit(const ExtendStmt *stmt) {
  result = format("{} {}:{}{}", keyword("extend"), transform(stmt->what),
                  newline(), transform(stmt->suite, 1));
}

void FormatVisitor::visit(const ImportStmt *stmt) {
  auto fix = [](const string &s) {
    string r = s;
    for (auto &c : r) {
      if (c == '/')
        c = '.';
    }
    return r;
  };
  if (stmt->what.size() == 0) {
    result = format("{} {}{}", keyword("import"), fix(stmt->from.first),
                    stmt->from.second == ""
                        ? ""
                        : format("{} {} ", keyword("as"), stmt->from.second));
  } else {
    vector<string> what;
    for (auto &w : stmt->what) {
      what.push_back(format(
          "{}{}", fix(w.first),
          w.second == "" ? "" : format("{} {} ", keyword("as"), w.second)));
    }
    result = format("{} {} {} {}", keyword("from"), fix(stmt->from.first),
                    keyword("import"), fmt::join(what, ", "));
  }
}

void FormatVisitor::visit(const ExternImportStmt *stmt) {
  this->result = "<extern: todo>";
}

void FormatVisitor::visit(const TryStmt *stmt) {
  vector<string> catches;
  for (auto &c : stmt->catches) {
    catches.push_back(
        format("{} {}{}:{}{}", keyword("catch"), transform(c.exc),
               c.var == "" ? "" : format("{} {}", keyword("as"), c.var),
               newline(), transform(c.suite, 1)));
  }
  result = format("{}:{}{}{}{}", keyword("try"), newline(),
                  transform(stmt->suite, 1), fmt::join(catches, ""),
                  stmt->finally ? format("{}:{}{}", keyword("finally"),
                                         newline(), transform(stmt->finally, 1))
                                : "");
}

void FormatVisitor::visit(const GlobalStmt *stmt) {
  result = format("{} {}", keyword("global"), stmt->var);
}

void FormatVisitor::visit(const ThrowStmt *stmt) {
  result = format("{} {}", keyword("raise"), transform(stmt->expr));
}

void FormatVisitor::visit(const FunctionStmt *stmt) {
  auto cn = ctx.getCanonicalName(stmt->getSrcInfo());
  // result += renderComment("# DEF-FUN {} CANONICAL {} TYPE {}", stmt->name,
  // cn, ctx.funcASTs[cn].first->str());
  for (auto &i : ctx.getRealizations(stmt)) {
    auto *fstmt = dynamic_cast<const FunctionStmt *>(i.second);
    assert(fstmt);
    string attrs;
    for (auto &a : fstmt->attributes) {
      attrs += format("{}@{}{}", pad(), a, newline());
    }
    vector<string> args;
    for (auto &a : fstmt->args) {
      args.push_back(format(
          "{}{}{}", a.name, a.type ? format(": {}", transform(a.type)) : "",
          a.deflt ? format(" = {}", transform(a.deflt)) : ""));
    }

    FormatVisitor v(ctx);
    v.indent = this->indent + 1;
    if (fstmt->suite)
      fstmt->suite->accept(v);
    result +=
        format("{}{}{} {}.{}{}({}){}:{}{}{}", attrs, pad(), keyword("def"),
               ctx.getCanonicalName(fstmt->getSrcInfo()), i.first,
               !fstmt->generics.empty()
                   ? format("[{}]", fmt::join(fstmt->generics, ", "))
                   : "",
               fmt::join(args, ", "),
               fstmt->ret ? format(" -> {}", transform(fstmt->ret)) : "",
               newline(), v.result, newline());
  }
}

void FormatVisitor::visit(const ClassStmt *stmt) {
  vector<string> args;
  for (auto &a : stmt->args) {
    args.push_back(format("{}{}{}", a.name,
                          a.type ? format(": {}", transform(a.type)) : "",
                          a.deflt ? format(" = {}", transform(a.deflt)) : ""));
  }

  if (stmt->isRecord) {
    auto t = transform(stmt->suite, 1);
    result = format("{} {}({}){}", keyword("type"), stmt->name,
                    fmt::join(args, ", "),
                    !t.empty() ? format(":{}{}", newline(), t) : "");

  } else {
    string as;
    for (auto &a : args)
      as += pad(1) + a + newline();
    result = format("{} {}{}:{}{}{}", keyword("class"), stmt->name,
                    !stmt->generics.empty()
                        ? format("[{}]", fmt::join(stmt->generics, ", "))
                        : "",
                    newline(), as, transform(stmt->suite, 1));
  }
}

void FormatVisitor::visit(const DeclareStmt *stmt) {
  result = format("{}: {}", stmt->param.name, transform(stmt->param.type));
}

void FormatVisitor::visit(const AssignEqStmt *stmt) {
  result =
      format("{} {}= {}", transform(stmt->lhs), stmt->op, transform(stmt->rhs));
}

void FormatVisitor::visit(const YieldFromStmt *stmt) {
  result = format("{} {}", keyword("yield from"), transform(stmt->expr));
}

void FormatVisitor::visit(const WithStmt *stmt) {
  vector<string> what;
  for (int wi = 0; wi < stmt->items.size(); wi++) {
    what.push_back(
        format("{}{}", *stmt->items[wi],
               stmt->vars[wi] == "" ? "" : format(" as {}", stmt->vars[wi])));
  }
  result = format("{} {}:{}{}", keyword("with"), fmt::join(what, ", "),
                  newline(), transform(stmt->suite, 1));
}

void FormatVisitor::visit(const PyDefStmt *stmt) {
  result = "<pydef_fn: todo>";
}

void FormatVisitor::visit(const StarPattern *pat) { this->result = "..."; }

void FormatVisitor::visit(const IntPattern *pat) {
  result = format("{}", pat->value);
}

void FormatVisitor::visit(const BoolPattern *pat) {
  result = format("{}", pat->value ? "True" : "False");
}

void FormatVisitor::visit(const StrPattern *pat) {
  result = format("\"{}\"", escape(pat->value));
}

void FormatVisitor::visit(const SeqPattern *pat) {
  result = format("s\"{}\"", escape(pat->value));
}

void FormatVisitor::visit(const RangePattern *pat) {
  result = format("{} ... {}", pat->start, pat->end);
}

void FormatVisitor::visit(const TuplePattern *pat) {
  string r;
  for (auto &e : pat->patterns)
    r += transform(e) + ", ";
  result = format("({})", r);
}

void FormatVisitor::visit(const ListPattern *pat) {
  string r;
  for (auto &e : pat->patterns)
    r += transform(e) + ", ";
  result = format("[{}}]", r);
}

void FormatVisitor::visit(const OrPattern *pat) {
  vector<string> r;
  for (auto &e : pat->patterns)
    r.push_back(format("({})", transform(e)));
  result = format("{}", fmt::join(r, keyword(" or ")));
}

void FormatVisitor::visit(const WildcardPattern *pat) {
  result = format("{}", pat->var == "" ? "_" : pat->var);
}

void FormatVisitor::visit(const GuardedPattern *pat) {
  // TODO
  // result = format("{} if {}", transform(pat->pattern),
  //                FormatVisitor(ctx).transform(pat->cond));
}

void FormatVisitor::visit(const BoundPattern *pat) {
  result =
      format("({}) {} {}", transform(pat->pattern), keyword("as"), pat->var);
}

} // namespace ast
} // namespace seq
