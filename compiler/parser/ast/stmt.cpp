/*
 * stmt.cpp --- Seq AST statements.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <memory>
#include <string>
#include <vector>

#include "parser/ast.h"
#include "parser/visitors/visitor.h"

#define ACCEPT_IMPL(T, X)                                                              \
  StmtPtr T::clone() const { return make_unique<T>(*this); }                           \
  void T::accept(X &visitor) { visitor.visit(this); }

using fmt::format;
using std::move;

const int INDENT_SIZE = 2;

namespace seq {
namespace ast {

Stmt::Stmt() : done(false), age(-1) {}
Stmt::Stmt(const seq::SrcInfo &s) : done(false) { setSrcInfo(s); }
string Stmt::toString() const { return toString(-1); }

SuiteStmt::SuiteStmt(vector<StmtPtr> &&stmts, bool ownBlock)
    : Stmt(), ownBlock(ownBlock) {
  for (auto &s : stmts)
    flatten(move(s), this->stmts);
}
SuiteStmt::SuiteStmt(StmtPtr stmt, bool ownBlock) : Stmt(), ownBlock(ownBlock) {
  flatten(move(stmt), this->stmts);
}
SuiteStmt::SuiteStmt(StmtPtr stmt1, StmtPtr stmt2, bool ownBlock)
    : Stmt(), ownBlock(ownBlock) {
  flatten(move(stmt1), this->stmts);
  flatten(move(stmt2), this->stmts);
}
SuiteStmt::SuiteStmt(StmtPtr stmt1, StmtPtr stmt2, StmtPtr stmt3, bool o)
    : Stmt(), ownBlock(o) {
  flatten(move(stmt1), this->stmts);
  flatten(move(stmt2), this->stmts);
  flatten(move(stmt3), this->stmts);
}
SuiteStmt::SuiteStmt() : Stmt(), ownBlock(false) {}
SuiteStmt::SuiteStmt(const SuiteStmt &stmt)
    : Stmt(stmt), stmts(ast::clone(stmt.stmts)), ownBlock(stmt.ownBlock) {}
string SuiteStmt::toString(int indent) const {
  string pad = indent >= 0 ? ("\n" + string(indent + INDENT_SIZE, ' ')) : " ";
  string s;
  for (int i = 0; i < stmts.size(); i++)
    if (stmts[i])
      s += (i ? pad : "") + stmts[i]->toString(indent >= 0 ? indent + INDENT_SIZE : -1);
  return format("(suite {}{}{})", ownBlock ? "#:own " : "", pad, s);
}
ACCEPT_IMPL(SuiteStmt, ASTVisitor);
void SuiteStmt::flatten(StmtPtr s, vector<StmtPtr> &stmts) {
  if (!s)
    return;
  auto suite = const_cast<SuiteStmt *>(s->getSuite());
  if (!suite || suite->ownBlock)
    stmts.push_back(move(s));
  else {
    for (auto &ss : suite->stmts)
      stmts.push_back(move(ss));
  }
}

string PassStmt::toString(int) const { return "(pass)"; }
ACCEPT_IMPL(PassStmt, ASTVisitor);

string BreakStmt::toString(int) const { return "(break)"; }
ACCEPT_IMPL(BreakStmt, ASTVisitor);

string ContinueStmt::toString(int) const { return "(continue)"; }
ACCEPT_IMPL(ContinueStmt, ASTVisitor);

ExprStmt::ExprStmt(ExprPtr expr) : Stmt(), expr(move(expr)) {}
ExprStmt::ExprStmt(const ExprStmt &stmt) : Stmt(stmt), expr(ast::clone(stmt.expr)) {}
string ExprStmt::toString(int) const { return format("(expr {})", expr->toString()); }
ACCEPT_IMPL(ExprStmt, ASTVisitor);

AssignStmt::AssignStmt(ExprPtr lhs, ExprPtr rhs, ExprPtr type, bool shadow)
    : Stmt(), lhs(move(lhs)), rhs(move(rhs)), type(move(type)), shadow(shadow) {}
AssignStmt::AssignStmt(const AssignStmt &stmt)
    : Stmt(stmt), lhs(ast::clone(stmt.lhs)), rhs(ast::clone(stmt.rhs)),
      type(ast::clone(stmt.type)), shadow(stmt.shadow) {}
string AssignStmt::toString(int) const {
  return format("(assign {}{}{})", lhs->toString(), rhs ? " " + rhs->toString() : "",
                type ? format(" #:type {}", type->toString()) : "");
}
ACCEPT_IMPL(AssignStmt, ASTVisitor);

DelStmt::DelStmt(ExprPtr expr) : Stmt(), expr(move(expr)) {}
DelStmt::DelStmt(const DelStmt &stmt) : Stmt(stmt), expr(ast::clone(stmt.expr)) {}
string DelStmt::toString(int) const { return format("(del {})", expr->toString()); }
ACCEPT_IMPL(DelStmt, ASTVisitor);

PrintStmt::PrintStmt(vector<ExprPtr> &&items, bool isInline)
    : Stmt(), items(move(items)), isInline(isInline) {}
PrintStmt::PrintStmt(const PrintStmt &stmt)
    : Stmt(stmt), items(ast::clone(stmt.items)), isInline(stmt.isInline) {}
string PrintStmt::toString(int) const {
  return format("(print {}{})", isInline ? "#:inline " : "", combine(items));
}
ACCEPT_IMPL(PrintStmt, ASTVisitor);

ReturnStmt::ReturnStmt(ExprPtr expr) : Stmt(), expr(move(expr)) {}
ReturnStmt::ReturnStmt(const ReturnStmt &stmt)
    : Stmt(stmt), expr(ast::clone(stmt.expr)) {}
string ReturnStmt::toString(int) const {
  return expr ? format("(return {})", expr->toString()) : "(return)";
}
ACCEPT_IMPL(ReturnStmt, ASTVisitor);

YieldStmt::YieldStmt(ExprPtr expr) : Stmt(), expr(move(expr)) {}
YieldStmt::YieldStmt(const YieldStmt &stmt) : Stmt(stmt), expr(ast::clone(stmt.expr)) {}
string YieldStmt::toString(int) const {
  return expr ? format("(yield {})", expr->toString()) : "(yield)";
}
ACCEPT_IMPL(YieldStmt, ASTVisitor);

AssertStmt::AssertStmt(ExprPtr expr, ExprPtr message)
    : Stmt(), expr(move(expr)), message(move(message)) {}
AssertStmt::AssertStmt(const AssertStmt &stmt)
    : Stmt(stmt), expr(ast::clone(stmt.expr)), message(ast::clone(stmt.message)) {}
string AssertStmt::toString(int) const {
  return format("(assert {}{})", expr->toString(), message ? message->toString() : "");
}
ACCEPT_IMPL(AssertStmt, ASTVisitor);

WhileStmt::WhileStmt(ExprPtr cond, StmtPtr suite, StmtPtr elseSuite)
    : Stmt(), cond(move(cond)), suite(move(suite)), elseSuite(move(elseSuite)) {}
WhileStmt::WhileStmt(const WhileStmt &stmt)
    : Stmt(stmt), cond(ast::clone(stmt.cond)), suite(ast::clone(stmt.suite)),
      elseSuite(ast::clone(stmt.elseSuite)) {}
string WhileStmt::toString(int indent) const {
  string pad = indent > 0 ? ("\n" + string(indent + INDENT_SIZE, ' ')) : " ";
  if (elseSuite && elseSuite->firstInBlock())
    return format("(while-else {}{}{}{}{})", cond->toString(), pad,
                  suite->toString(indent >= 0 ? indent + INDENT_SIZE : -1), pad,
                  elseSuite->toString(indent >= 0 ? indent + INDENT_SIZE : -1));
  else
    return format("(while {}{}{})", cond->toString(), pad,
                  suite->toString(indent >= 0 ? indent + INDENT_SIZE : -1));
}
ACCEPT_IMPL(WhileStmt, ASTVisitor);

ForStmt::ForStmt(ExprPtr var, ExprPtr iter, StmtPtr suite, StmtPtr elseSuite)
    : Stmt(), var(move(var)), iter(move(iter)), suite(move(suite)),
      elseSuite(move(elseSuite)), wrapped(false) {}
ForStmt::ForStmt(const ForStmt &stmt)
    : Stmt(stmt), var(ast::clone(stmt.var)), iter(ast::clone(stmt.iter)),
      suite(ast::clone(stmt.suite)), elseSuite(ast::clone(stmt.elseSuite)),
      wrapped(stmt.wrapped) {}
string ForStmt::toString(int indent) const {
  string pad = indent > 0 ? ("\n" + string(indent + INDENT_SIZE, ' ')) : " ";
  if (elseSuite && elseSuite->firstInBlock())
    return format("(for-else {} {}{}{}{}{})", var->toString(), iter->toString(), pad,
                  suite->toString(indent >= 0 ? indent + INDENT_SIZE : -1), pad,
                  elseSuite->toString(indent >= 0 ? indent + INDENT_SIZE : -1));
  else
    return format("(for {} {}{}{})", var->toString(), iter->toString(), pad,
                  suite->toString(indent >= 0 ? indent + INDENT_SIZE : -1));
}
ACCEPT_IMPL(ForStmt, ASTVisitor);

IfStmt::IfStmt(ExprPtr cond, StmtPtr ifSuite, StmtPtr elseSuite)
    : Stmt(), cond(move(cond)), ifSuite(move(ifSuite)), elseSuite(move(elseSuite)) {}
IfStmt::IfStmt(const IfStmt &stmt)
    : Stmt(stmt), cond(ast::clone(stmt.cond)), ifSuite(ast::clone(stmt.ifSuite)),
      elseSuite(ast::clone(stmt.elseSuite)) {}
string IfStmt::toString(int indent) const {
  string pad = indent > 0 ? ("\n" + string(indent + INDENT_SIZE, ' ')) : " ";
  return format("(if {}{}{}{})", cond->toString(), pad,
                ifSuite->toString(indent >= 0 ? indent + INDENT_SIZE : -1),
                elseSuite
                    ? pad + elseSuite->toString(indent >= 0 ? indent + INDENT_SIZE : -1)
                    : "");
}
ACCEPT_IMPL(IfStmt, ASTVisitor);

MatchStmt::MatchCase MatchStmt::MatchCase::clone() const {
  return {ast::clone(pattern), ast::clone(guard), ast::clone(suite)};
}

MatchStmt::MatchStmt(ExprPtr what, vector<MatchStmt::MatchCase> &&cases)
    : Stmt(), what(move(what)), cases(move(cases)) {}
MatchStmt::MatchStmt(const MatchStmt &stmt)
    : Stmt(stmt), what(ast::clone(stmt.what)), cases(ast::clone_nop(stmt.cases)) {}
string MatchStmt::toString(int indent) const {
  string pad = indent > 0 ? ("\n" + string(indent + INDENT_SIZE, ' ')) : " ";
  string padExtra = indent > 0 ? string(INDENT_SIZE, ' ') : "";
  vector<string> s;
  for (auto &c : cases)
    s.push_back(format("(case {}{}{}{})", c.pattern->toString(),
                       c.guard ? " #:guard " + c.guard->toString() : "", pad + padExtra,
                       c.suite->toString(indent >= 0 ? indent + INDENT_SIZE : -1 * 2)));
  return format("(match {}{}{})", what->toString(), pad, join(s, pad));
}
ACCEPT_IMPL(MatchStmt, ASTVisitor);

ImportStmt::ImportStmt(ExprPtr from, ExprPtr what, vector<Param> &&args, ExprPtr ret,
                       string as, int dots)
    : Stmt(), from(move(from)), what(move(what)), as(move(as)), dots(dots),
      args(move(args)), ret(move(ret)) {}
ImportStmt::ImportStmt(const ImportStmt &stmt)
    : Stmt(stmt), from(ast::clone(stmt.from)), what(ast::clone(stmt.what)), as(stmt.as),
      dots(stmt.dots), args(ast::clone_nop(stmt.args)), ret(ast::clone(stmt.ret)) {}
string ImportStmt::toString(int) const {
  return format("(import {}{}{}{})", what->toString(),
                as.empty() ? "" : format(" #:as '{}", as),
                from ? format(" #:from {}", from->toString()) : "",
                dots ? format(" #:dots {}", dots) : "",
                ret ? format(" #:ret {}", ret->toString()) : "");
}
ACCEPT_IMPL(ImportStmt, ASTVisitor);

TryStmt::Catch TryStmt::Catch::clone() const {
  return {var, ast::clone(exc), ast::clone(suite)};
}

TryStmt::TryStmt(StmtPtr suite, vector<Catch> &&catches, StmtPtr finally)
    : Stmt(), suite(move(suite)), catches(move(catches)), finally(move(finally)) {}
TryStmt::TryStmt(const TryStmt &stmt)
    : Stmt(stmt), suite(ast::clone(stmt.suite)), catches(ast::clone_nop(stmt.catches)),
      finally(ast::clone(stmt.finally)) {}
string TryStmt::toString(int indent) const {
  string pad = indent > 0 ? ("\n" + string(indent + INDENT_SIZE, ' ')) : " ";
  string padExtra = indent > 0 ? string(INDENT_SIZE, ' ') : "";
  vector<string> s;
  for (auto &i : catches)
    s.push_back(
        format("(catch {}{}{}{})", !i.var.empty() ? format("#:var '{}", i.var) : "",
               i.exc ? format(" #:exc {}", i.exc->toString()) : "", pad + padExtra,
               i.suite->toString(indent >= 0 ? indent + INDENT_SIZE : -1 * 2)));
  auto f =
      format("{}{}", pad, finally->toString(indent >= 0 ? indent + INDENT_SIZE : -1));
  return format(
      "(try{}{}{}{}{})", pad, suite->toString(indent >= 0 ? indent + INDENT_SIZE : -1),
      pad, join(s, pad),
      finally ? format("{}{}", pad,
                       finally->toString(indent >= 0 ? indent + INDENT_SIZE : -1))
              : "");
}
ACCEPT_IMPL(TryStmt, ASTVisitor);

ThrowStmt::ThrowStmt(ExprPtr expr, bool transformed)
    : Stmt(), expr(move(expr)), transformed(transformed) {}
ThrowStmt::ThrowStmt(const ThrowStmt &stmt)
    : Stmt(stmt), expr(ast::clone(stmt.expr)), transformed(stmt.transformed) {}
string ThrowStmt::toString(int) const {
  return format("(throw{})", expr ? " " + expr->toString() : "");
}
ACCEPT_IMPL(ThrowStmt, ASTVisitor);

GlobalStmt::GlobalStmt(string var) : Stmt(), var(move(var)) {}
string GlobalStmt::toString(int) const { return format("(global '{})", var); }
ACCEPT_IMPL(GlobalStmt, ASTVisitor);

Attr::Attr(const vector<string> &attrs) : module(), parentClass(), isAttribute(false) {
  for (auto &a : attrs)
    set(a);
}
void Attr::set(const string &attr) { customAttr.insert(attr); }
void Attr::unset(const string &attr) { customAttr.erase(attr); }
bool Attr::has(const string &attr) const { return in(customAttr, attr); }

const string Attr::LLVM = "llvm";
const string Attr::Python = "python";
const string Attr::Atomic = "atomic";
const string Attr::Property = "property";
const string Attr::Internal = "__internal__";
const string Attr::ForceRealize = "__force__";
const string Attr::C = ".__c__";
const string Attr::Method = ".__method__";
const string Attr::Capture = ".__capture__";
const string Attr::Extend = "extend";
const string Attr::Tuple = "tuple";
const string Attr::Test = "std.internal.attributes.test";

FunctionStmt::FunctionStmt(string name, ExprPtr ret, vector<Param> &&generics,
                           vector<Param> &&args, StmtPtr suite, Attr attributes,
                           vector<ExprPtr> &&decorators)
    : Stmt(), name(move(name)), ret(move(ret)), generics(move(generics)),
      args(move(args)), suite(move(suite)), attributes(move(attributes)),
      decorators(move(decorators)) {}
FunctionStmt::FunctionStmt(const FunctionStmt &stmt)
    : Stmt(stmt), name(stmt.name), ret(ast::clone(stmt.ret)),
      generics(ast::clone_nop(stmt.generics)), args(ast::clone_nop(stmt.args)),
      suite(ast::clone(stmt.suite)), attributes(stmt.attributes),
      decorators(ast::clone(stmt.decorators)) {}
string FunctionStmt::toString(int indent) const {
  string pad = indent > 0 ? ("\n" + string(indent + INDENT_SIZE, ' ')) : " ";
  string gs;
  for (auto &a : generics)
    gs += " " + a.toString();
  string as;
  for (auto &a : args)
    as += " " + a.toString();
  vector<string> attr;
  for (auto &a : decorators)
    attr.push_back(format("(dec {})", a->toString()));
  return format(
      "(fn '{} ({}){}{} (attr {}){}{})", name, as,
      ret ? " #:ret " + ret->toString() : "",
      !generics.empty() ? format(" #:generics ({})", gs) : "", join(attr, " "), pad,
      suite ? suite->toString(indent >= 0 ? indent + INDENT_SIZE : -1) : "(pass)");
}
ACCEPT_IMPL(FunctionStmt, ASTVisitor);
string FunctionStmt::signature() const {
  vector<string> s;
  for (auto &a : generics)
    s.push_back(a.name);
  for (auto &a : args)
    s.push_back(a.type ? a.type->toString() : "-");
  return format("{}", join(s, ":"));
}
bool FunctionStmt::hasAttr(const string &attr) const { return attributes.has(attr); }

ClassStmt::ClassStmt(string name, vector<Param> &&g, vector<Param> &&a, StmtPtr s,
                     Attr attributes, vector<ExprPtr> &&decorators)
    : Stmt(), name(move(name)), generics(move(g)), args(move(a)), suite(move(s)),
      attributes(move(attributes)), decorators(move(decorators)) {}
ClassStmt::ClassStmt(const ClassStmt &stmt)
    : Stmt(stmt), name(stmt.name), generics(ast::clone_nop(stmt.generics)),
      args(ast::clone_nop(stmt.args)), suite(ast::clone(stmt.suite)),
      attributes(stmt.attributes), decorators(ast::clone(stmt.decorators)) {}
string ClassStmt::toString(int indent) const {
  string pad = indent > 0 ? ("\n" + string(indent + INDENT_SIZE, ' ')) : " ";
  string gs;
  for (auto &a : generics)
    gs += " " + a.toString();
  string as;
  for (auto &a : args)
    as += " " + a.toString();
  vector<string> attr;
  for (auto &a : decorators)
    attr.push_back(format("(dec {})", a->toString()));
  return format(
      "(class '{} ({}){} (attr {}){}{})", name, as,
      !generics.empty() ? format(" #:generics ({})", gs) : "", join(attr, " "), pad,
      suite ? suite->toString(indent >= 0 ? indent + INDENT_SIZE : -1) : "(pass)");
}
ACCEPT_IMPL(ClassStmt, ASTVisitor);
bool ClassStmt::isRecord() const { return hasAttr(Attr::Tuple); }
bool ClassStmt::hasAttr(const string &attr) const { return attributes.has(attr); }

YieldFromStmt::YieldFromStmt(ExprPtr expr) : Stmt(), expr(move(expr)) {}
YieldFromStmt::YieldFromStmt(const YieldFromStmt &stmt)
    : Stmt(stmt), expr(ast::clone(stmt.expr)) {}
string YieldFromStmt::toString(int) const {
  return format("(yield-from {})", expr->toString());
}
ACCEPT_IMPL(YieldFromStmt, ASTVisitor);

WithStmt::WithStmt(vector<ExprPtr> &&items, vector<string> &&vars, StmtPtr suite)
    : Stmt(), items(move(items)), vars(vars), suite(move(suite)) {
  assert(items.size() == vars.size());
}
WithStmt::WithStmt(vector<pair<ExprPtr, string>> &&itemVarPairs, StmtPtr suite)
    : Stmt(), suite(move(suite)) {
  for (auto &i : itemVarPairs) {
    items.push_back(move(i.first));
    vars.push_back(i.second);
  }
}
WithStmt::WithStmt(const WithStmt &stmt)
    : Stmt(stmt), items(ast::clone(stmt.items)), vars(stmt.vars),
      suite(ast::clone(stmt.suite)) {}
string WithStmt::toString(int indent) const {
  string pad = indent > 0 ? ("\n" + string(indent + INDENT_SIZE, ' ')) : " ";
  vector<string> as;
  for (int i = 0; i < items.size(); i++) {
    as.push_back(!vars[i].empty()
                     ? format("({} #:var '{})", items[i]->toString(), vars[i])
                     : items[i]->toString());
  }
  return format("(with ({}){}{})", join(as, " "), pad,
                suite->toString(indent >= 0 ? indent + INDENT_SIZE : -1));
}
ACCEPT_IMPL(WithStmt, ASTVisitor);

CustomStmt::CustomStmt(ExprPtr head, StmtPtr suite)
    : Stmt(), head(move(head)), suite(move(suite)) {}
CustomStmt::CustomStmt(const CustomStmt &stmt)
    : Stmt(stmt), head(ast::clone(stmt.head)), suite(ast::clone(stmt.suite)) {}
string CustomStmt::toString(int indent) const {
  string pad = indent > 0 ? ("\n" + string(indent + INDENT_SIZE, ' ')) : " ";
  return format("(custom {}{}{})", head->toString(), pad,
                suite->toString(indent >= 0 ? indent + INDENT_SIZE : -1));
}
ACCEPT_IMPL(CustomStmt, ASTVisitor);

AssignMemberStmt::AssignMemberStmt(ExprPtr lhs, string member, ExprPtr rhs)
    : Stmt(), lhs(move(lhs)), member(move(member)), rhs(move(rhs)) {}
AssignMemberStmt::AssignMemberStmt(const AssignMemberStmt &stmt)
    : Stmt(stmt), lhs(ast::clone(stmt.lhs)), member(stmt.member),
      rhs(ast::clone(stmt.rhs)) {}
string AssignMemberStmt::toString(int) const {
  return format("(assign-member {} {} {})", lhs->toString(), member, rhs->toString());
}
ACCEPT_IMPL(AssignMemberStmt, ASTVisitor);

UpdateStmt::UpdateStmt(ExprPtr lhs, ExprPtr rhs, bool isAtomic)
    : Stmt(), lhs(move(lhs)), rhs(move(rhs)), isAtomic(isAtomic) {}
UpdateStmt::UpdateStmt(const UpdateStmt &stmt)
    : Stmt(stmt), lhs(ast::clone(stmt.lhs)), rhs(ast::clone(stmt.rhs)),
      isAtomic(stmt.isAtomic) {}
string UpdateStmt::toString(int) const {
  return format("(update {} {})", lhs->toString(), rhs->toString());
}
ACCEPT_IMPL(UpdateStmt, ASTVisitor);

} // namespace ast
} // namespace seq
