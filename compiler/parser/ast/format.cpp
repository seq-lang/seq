#include <ostream>
#include <string>
#include <unordered_set>
#include <vector>

#include "parser/ast/format.h"

using fmt::format;
using std::string;
using std::unordered_set;
using std::vector;

namespace seq {
namespace ast {

FormatVisitor::FormatVisitor(std::shared_ptr<TypeContext> ctx, bool html)
    : ctx(ctx), renderType(false), renderHTML(html), indent(0) {
  if (renderHTML) {
    header = "<html><head><link rel=stylesheet href=code.css/></head>\n<body>";
    header += "<div class=code>\n";
    footer = "\n</div></body></html>";
    nl = "<hr>";
    typeStart = "<type>";
    typeEnd = "</type>";
    nodeStart = "<node>";
    nodeEnd = "</node>";
    exprStart = "<expr>";
    exprEnd = "</expr>";
    commentStart = "<b>";
    commentEnd = "</b>";
    keywordStart = "<b class=comment>";
    keywordEnd = "</b>";
    space = "&nbsp";
    renderType = true;
  } else {
    space = " ";
  }
}

string FormatVisitor::handleImport(const string &what, const string &file) {
  static unordered_set<string> sp;
  if (sp.find(file) != sp.end())
    return "";
  sp.insert(file);
  auto s = ctx->getImports()->getImport(file);
  auto old = ctx->getFilename();
  ctx->setFilename(s->filename);
  auto t =
      FormatVisitor(ctx, renderHTML).transform(s->statements.get(), indent + 1);
  ctx->setFilename(old);
  return fmt::format("{} {}: # {}{}{}{}", keyword("%import"), what,
                     escape(s->filename), newline(), pad(1), t);
}

string FormatVisitor::transform(const ExprPtr &expr) {
  FormatVisitor v(ctx, renderHTML);
  if (expr)
    expr->accept(v);
  return v.result;
}

string FormatVisitor::transform(const Stmt *stmt, int indent) {
  FormatVisitor v(ctx, renderHTML);
  v.indent = this->indent + indent;
  if (stmt)
    stmt->accept(v);
  if (v.result.size())
    return fmt::format("{}{}{}", pad(), v.result, newline());
  else
    return "";
}

string FormatVisitor::transform(const PatternPtr &ptr) {
  FormatVisitor v(ctx, renderHTML);
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

string FormatVisitor::newline() const { return nl + "\n"; }

string FormatVisitor::keyword(const string &s) const {
  return fmt::format("{}{}{}", keywordStart, s, keywordEnd);
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
    s.push_back(fmt::format("{}: {}", transform(i.key), transform(i.value)));
  result = renderExpr(expr, "{{{}}}", join(s, ", "));
}

void FormatVisitor::visit(const GeneratorExpr *expr) {
  string s;
  for (auto &i : expr->loops) {
    string cond;
    for (auto &k : i.conds)
      cond += fmt::format(" if {}", transform(k));
    s += fmt::format("for {} in {}{}", fmt::join(i.vars, ", "),
                     i.gen->toString(), cond);
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
      cond += fmt::format(" if {}", transform(k));

    s += fmt::format("for {} in {}{}", fmt::join(i.vars, ", "),
                     i.gen->toString(), cond);
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
      args.push_back(fmt::format("{}: {}", i.name, transform(i.value)));
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
  for (int i = 0; i < stmt->stmts.size(); i++)
    result += transform(stmt->stmts[i]);
  if (result.size()) {
    result = result.substr(pad().size());
    result = result.substr(0, result.size() - newline().size());
  }
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
    result = fmt::format("{}: {} = {}", transform(stmt->lhs),
                         transform(stmt->type), transform(stmt->rhs));
  } else if (stmt->mustExist) {
    result = fmt::format("{}", transform(stmt->rhs));
  } else {
    result = fmt::format("{} = {}", transform(stmt->lhs), transform(stmt->rhs));
  }
}

void FormatVisitor::visit(const DelStmt *stmt) {
  result = fmt::format("{} {}", keyword("del"), transform(stmt->expr));
}

void FormatVisitor::visit(const PrintStmt *stmt) {
  result = fmt::format("{} {}", keyword("print"), transform(stmt->expr));
}

void FormatVisitor::visit(const ReturnStmt *stmt) {
  result = fmt::format("{}{}", keyword("return"),
                       stmt->expr ? " " + transform(stmt->expr) : "");
}

void FormatVisitor::visit(const YieldStmt *stmt) {
  result = fmt::format("{}{}", keyword("yield"),
                       stmt->expr ? " " + transform(stmt->expr) : "");
}

void FormatVisitor::visit(const AssertStmt *stmt) {
  result = fmt::format("{} {}", keyword("assert"), transform(stmt->expr));
}

void FormatVisitor::visit(const WhileStmt *stmt) {
  result = fmt::format("{} {}:{}{}{}", keyword("while"), transform(stmt->cond),
                       newline(), pad(1), transform(stmt->suite, 1));
}

void FormatVisitor::visit(const ForStmt *stmt) {
  result = fmt::format(
      "{} {} {} {}:{}{}{}", keyword("for"), transform(stmt->var), keyword("in"),
      transform(stmt->iter), newline(), pad(1), transform(stmt->suite, 1));
}

void FormatVisitor::visit(const IfStmt *stmt) {
  string ifs;
  string prefix = "";
  for (int i = 0; i < stmt->ifs.size(); i++) {
    if (stmt->ifs[i].cond)
      ifs += fmt::format("{}{} {}:{}{}{}", i ? pad() : "",
                         keyword(prefix + "if"), transform(stmt->ifs[i].cond),
                         newline(), pad(1), transform(stmt->ifs[i].suite, 1));
    else
      ifs += fmt::format("{}{}:{}{}{}", pad(), keyword("else"), newline(),
                         pad(1), transform(stmt->ifs[i].suite, 1));
    prefix = "el";
  }
  result = ifs;
}

void FormatVisitor::visit(const MatchStmt *stmt) {
  string s;
  for (int ci = 0; ci < stmt->cases.size(); ci++)
    s += fmt::format("{}{}{}:{}{}{}{}", pad(1), keyword("case"),
                     transform(stmt->patterns[ci]), newline(), pad(2),
                     transform(stmt->cases[ci], 2),
                     ci == stmt->cases.size() - 1 ? "" : newline());
  result = fmt::format("{} {}:{}{}", keyword("match"), transform(stmt->what),
                       newline(), s);
}

void FormatVisitor::visit(const ExtendStmt *stmt) {}

void FormatVisitor::visit(const ImportStmt *stmt) {
  auto fix = [](const string &s) {
    string r = s;
    for (auto &c : r)
      if (c == '/')
        c = '.';
    return r;
  };

  if (stmt->what.size() == 0) {
    result += fmt::format(
        "{} {}{}", keyword("import"), fix(stmt->from.first),
        stmt->from.second == ""
            ? ""
            : fmt::format(" {} {} ", keyword("as"), stmt->from.second));
  } else {
    vector<string> what;
    for (auto &w : stmt->what) {
      what.push_back(fmt::format(
          "{}{}", fix(w.first),
          w.second == "" ? ""
                         : fmt::format(" {} {} ", keyword("as"), w.second)));
    }
    result += fmt::format("{} {} {} {}", keyword("from"), fix(stmt->from.first),
                          keyword("import"), fmt::join(what, ", "));
  }

  if (ctx) {
    auto file =
        ctx->getImports()->getImportFile(stmt->from.first, ctx->getFilename());
    auto i = handleImport(stmt->from.first, file);
    if (!i.empty())
      result += fmt::format("{}{}{}{}", newline(), pad(), i, newline());
  }
}

void FormatVisitor::visit(const ExternImportStmt *stmt) {}

void FormatVisitor::visit(const TryStmt *stmt) {
  vector<string> catches;
  for (auto &c : stmt->catches) {
    catches.push_back(fmt::format(
        "{} {}{}:{}{}{}", keyword("catch"), transform(c.exc),
        c.var == "" ? "" : fmt::format("{} {}", keyword("as"), c.var),
        newline(), pad(1), transform(c.suite, 1)));
  }
  result = fmt::format(
      "{}:{}{}{}{}{}", keyword("try"), newline(), pad(1),
      transform(stmt->suite, 1), fmt::join(catches, ""),
      stmt->finally ? fmt::format("{}:{}{}{}", keyword("finally"), newline(),
                                  pad(1), transform(stmt->finally, 1))
                    : "");
}

void FormatVisitor::visit(const GlobalStmt *stmt) {
  result = fmt::format("{} {}", keyword("global"), stmt->var);
}

void FormatVisitor::visit(const ThrowStmt *stmt) {
  result = fmt::format("{} {}", keyword("raise"), transform(stmt->expr));
}

void FormatVisitor::visit(const FunctionStmt *stmt) {
  if (!ctx)
    return;
  auto name = ctx->getRealizations()->getCanonicalName(stmt->getSrcInfo());
  auto fr = ctx->getRealizations()->getFuncRealizations(name);
  for (int fi = 0; fi < fr.size(); fi++) {
    auto &real = fr[fi];
    auto fstmt = real.ast;
    assert(fstmt);
    vector<string> attrs;
    for (auto &a : fstmt->attributes)
      attrs.push_back(fmt::format("@{}", a));
    vector<string> args;
    for (auto &a : fstmt->args)
      args.push_back(
          fmt::format("{}{}{}", a.name,
                      a.type ? fmt::format(": {}", transform(a.type)) : "",
                      a.deflt ? fmt::format(" = {}", transform(a.deflt)) : ""));
    vector<string> generics;
    for (auto &a : fstmt->generics)
      generics.push_back(
          fmt::format("{}{}{}", a.name,
                      a.type ? fmt::format(": {}", transform(a.type)) : "",
                      a.deflt ? fmt::format(" = {}", transform(a.deflt)) : ""));
    auto body = FormatVisitor(ctx, renderHTML)
                    .transform(fstmt->suite.get(), indent + 1);
    auto name = fmt::format("{}{}{}", typeStart, real.fullName, typeEnd);
    name = fmt::format("{}{}{}", exprStart, name, exprEnd);
    result += fmt::format(
        "{}{}{} {}{}({}){}:{}{}{}", fi ? newline() + pad() : "",
        attrs.size() ? join(attrs, ",") + newline() + pad() : "",
        keyword("def"), name,
        generics.size() ? fmt::format("[{}]", fmt::join(generics, ", ")) : "",
        fmt::join(args, ", "),
        fstmt->ret ? fmt::format(" -> {}", transform(fstmt->ret)) : "",
        newline(), pad(1),
        body.empty() ? fmt::format("{}", keyword("pass")) : body);
  }
}

void FormatVisitor::visit(const ClassStmt *stmt) {
  if (!ctx)
    return;
  auto name = ctx->getRealizations()->getCanonicalName(stmt->getSrcInfo());
  auto c = ctx->getRealizations()->findClass(name);
  assert(c);
  for (auto &real : ctx->getRealizations()->getClassRealizations(name)) {
    vector<string> args;
    string key = real.type->isRecord() ? "type" : "class";
    for (auto &a : c->members) {
      auto t = ctx->instantiate(real.type->getSrcInfo(), a.second, real.type);
      args.push_back(fmt::format("{}: {}", a.first, *t));
    }
    result = fmt::format("{} {}({})", keyword(key), real.fullName,
                         fmt::join(args, ", "));
  }
  bool added = 0;
  for (auto &m : c->methods) {
    auto s =
        FormatVisitor(ctx, renderHTML)
            .transform(ctx->getRealizations()
                           ->getAST(m.second.front()->realizationInfo->name)
                           .get(),
                       indent);
    if (s.size()) {
      if (result.size()) {
        if (result.substr(result.size() - newline().size()) != newline())
          result += newline();
        result += pad();
      }
      result += s;
      added = 1;
    }
  }
}

void FormatVisitor::visit(const AssignEqStmt *stmt) {}

void FormatVisitor::visit(const YieldFromStmt *stmt) {
  result = fmt::format("{} {}", keyword("yield from"), transform(stmt->expr));
}

void FormatVisitor::visit(const WithStmt *stmt) {}

void FormatVisitor::visit(const PyDefStmt *stmt) {}

void FormatVisitor::visit(const StarPattern *pat) { this->result = "..."; }

void FormatVisitor::visit(const IntPattern *pat) {
  result = fmt::format("{}", pat->value);
}

void FormatVisitor::visit(const BoolPattern *pat) {
  result = fmt::format("{}", pat->value ? "True" : "False");
}

void FormatVisitor::visit(const StrPattern *pat) {
  result = fmt::format("\"{}\"", escape(pat->value));
}

void FormatVisitor::visit(const SeqPattern *pat) {
  result = fmt::format("s\"{}\"", escape(pat->value));
}

void FormatVisitor::visit(const RangePattern *pat) {
  result = fmt::format("{} ... {}", pat->start, pat->end);
}

void FormatVisitor::visit(const TuplePattern *pat) {
  string r;
  for (auto &e : pat->patterns)
    r += transform(e) + ", ";
  result = fmt::format("({})", r);
}

void FormatVisitor::visit(const ListPattern *pat) {
  string r;
  for (auto &e : pat->patterns)
    r += transform(e) + ", ";
  result = fmt::format("[{}}]", r);
}

void FormatVisitor::visit(const OrPattern *pat) {
  vector<string> r;
  for (auto &e : pat->patterns)
    r.push_back(fmt::format("({})", transform(e)));
  result = fmt::format("{}", fmt::join(r, keyword(" or ")));
}

void FormatVisitor::visit(const WildcardPattern *pat) {
  result = fmt::format("{}", pat->var == "" ? "_" : pat->var);
}

void FormatVisitor::visit(const GuardedPattern *pat) {
  // TODO
  // result = fmt::format("{} if {}", transform(pat->pattern),
  //                FormatVisitor(ctx).transform(pat->cond));
}

void FormatVisitor::visit(const BoundPattern *pat) {
  result = fmt::format("({}) {} {}", transform(pat->pattern), keyword("as"),
                       pat->var);
}

} // namespace ast
} // namespace seq
