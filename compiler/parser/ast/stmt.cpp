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
#include "parser/cache.h"
#include "parser/visitors/visitor.h"

using fmt::format;
using std::move;

namespace seq {
namespace ast {

Stmt::Stmt(const seq::SrcInfo &s) { setSrcInfo(s); }

SuiteStmt::SuiteStmt(vector<StmtPtr> &&stmts, bool ownBlock) : ownBlock(ownBlock) {
  for (auto &s : stmts)
    flatten(move(s), this->stmts);
}
SuiteStmt::SuiteStmt(StmtPtr stmt, bool ownBlock) : ownBlock(ownBlock) {
  flatten(move(stmt), this->stmts);
}
SuiteStmt::SuiteStmt(StmtPtr stmt1, StmtPtr stmt2, bool ownBlock) : ownBlock(ownBlock) {
  flatten(move(stmt1), this->stmts);
  flatten(move(stmt2), this->stmts);
}
SuiteStmt::SuiteStmt(StmtPtr stmt1, StmtPtr stmt2, StmtPtr stmt3, bool o)
    : ownBlock(o) {
  flatten(move(stmt1), this->stmts);
  flatten(move(stmt2), this->stmts);
  flatten(move(stmt3), this->stmts);
}
SuiteStmt::SuiteStmt() : ownBlock(false) {}
SuiteStmt::SuiteStmt(const SuiteStmt &stmt)
    : Stmt(stmt), stmts(ast::clone(stmt.stmts)), ownBlock(stmt.ownBlock) {}
string SuiteStmt::toString() const {
  return format("(suite {}{})", ownBlock ? "#:own " : "", combine(stmts, " "));
}
StmtPtr SuiteStmt::clone() const { return make_unique<SuiteStmt>(*this); }
void SuiteStmt::accept(ASTVisitor &visitor) const { visitor.visit(this); }
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

string PassStmt::toString() const { return "(pass)"; }
StmtPtr PassStmt::clone() const { return make_unique<PassStmt>(*this); }
void PassStmt::accept(ASTVisitor &visitor) const { visitor.visit(this); }

string BreakStmt::toString() const { return "(break)"; }
StmtPtr BreakStmt::clone() const { return make_unique<BreakStmt>(*this); }
void BreakStmt::accept(ASTVisitor &visitor) const { visitor.visit(this); }

string ContinueStmt::toString() const { return "(continue)"; }
StmtPtr ContinueStmt::clone() const { return make_unique<ContinueStmt>(*this); }
void ContinueStmt::accept(ASTVisitor &visitor) const { visitor.visit(this); }

ExprStmt::ExprStmt(ExprPtr expr) : expr(move(expr)) {}
ExprStmt::ExprStmt(const ExprStmt &stmt) : Stmt(stmt), expr(ast::clone(stmt.expr)) {}
string ExprStmt::toString() const { return format("(expr {})", expr->toString()); }
StmtPtr ExprStmt::clone() const { return make_unique<ExprStmt>(*this); }
void ExprStmt::accept(ASTVisitor &visitor) const { visitor.visit(this); }

AssignStmt::AssignStmt(ExprPtr lhs, ExprPtr rhs, ExprPtr type)
    : lhs(move(lhs)), rhs(move(rhs)), type(move(type)) {}
AssignStmt::AssignStmt(const AssignStmt &stmt)
    : Stmt(stmt), lhs(ast::clone(stmt.lhs)), rhs(ast::clone(stmt.rhs)),
      type(ast::clone(stmt.type)) {}
string AssignStmt::toString() const {
  return format("(assign {}{}{})", lhs->toString(), rhs ? " " + rhs->toString() : "",
                type ? format(" #:type {}", type->toString()) : "");
}
StmtPtr AssignStmt::clone() const { return make_unique<AssignStmt>(*this); }
void AssignStmt::accept(ASTVisitor &visitor) const { visitor.visit(this); }

DelStmt::DelStmt(ExprPtr expr) : expr(move(expr)) {}
DelStmt::DelStmt(const DelStmt &stmt) : Stmt(stmt), expr(ast::clone(stmt.expr)) {}
string DelStmt::toString() const { return format("(del {})", expr->toString()); }
StmtPtr DelStmt::clone() const { return make_unique<DelStmt>(*this); }
void DelStmt::accept(ASTVisitor &visitor) const { visitor.visit(this); }

PrintStmt::PrintStmt(ExprPtr expr) : expr(move(expr)) {}
PrintStmt::PrintStmt(const PrintStmt &stmt) : Stmt(stmt), expr(ast::clone(stmt.expr)) {}
string PrintStmt::toString() const { return format("(print {})", expr->toString()); }
StmtPtr PrintStmt::clone() const { return make_unique<PrintStmt>(*this); }
void PrintStmt::accept(ASTVisitor &visitor) const { visitor.visit(this); }

ReturnStmt::ReturnStmt(ExprPtr expr) : expr(move(expr)) {}
ReturnStmt::ReturnStmt(const ReturnStmt &stmt)
    : Stmt(stmt), expr(ast::clone(stmt.expr)) {}
string ReturnStmt::toString() const {
  return expr ? format("(return {})", expr->toString()) : "(return)";
}
StmtPtr ReturnStmt::clone() const { return make_unique<ReturnStmt>(*this); }
void ReturnStmt::accept(ASTVisitor &visitor) const { visitor.visit(this); }

YieldStmt::YieldStmt(ExprPtr expr) : expr(move(expr)) {}
YieldStmt::YieldStmt(const YieldStmt &stmt) : Stmt(stmt), expr(ast::clone(stmt.expr)) {}
string YieldStmt::toString() const {
  return expr ? format("(yield {})", expr->toString()) : "(yield)";
}
StmtPtr YieldStmt::clone() const { return make_unique<YieldStmt>(*this); }
void YieldStmt::accept(ASTVisitor &visitor) const { visitor.visit(this); }

AssertStmt::AssertStmt(ExprPtr expr, ExprPtr message)
    : expr(move(expr)), message(move(message)) {}
AssertStmt::AssertStmt(const AssertStmt &stmt)
    : Stmt(stmt), expr(ast::clone(stmt.expr)), message(ast::clone(stmt.message)) {}
string AssertStmt::toString() const {
  return format("(assert {}{})", expr->toString(),
                message ? " #:msg \"" + message->toString() : "\"");
}
StmtPtr AssertStmt::clone() const { return make_unique<AssertStmt>(*this); }
void AssertStmt::accept(ASTVisitor &visitor) const { visitor.visit(this); }

WhileStmt::WhileStmt(ExprPtr cond, StmtPtr suite, StmtPtr elseSuite)
    : cond(move(cond)), suite(move(suite)), elseSuite(move(elseSuite)) {}
WhileStmt::WhileStmt(const WhileStmt &stmt)
    : Stmt(stmt), cond(ast::clone(stmt.cond)), suite(ast::clone(stmt.suite)),
      elseSuite(ast::clone(stmt.elseSuite)) {}
string WhileStmt::toString() const {
  if (elseSuite && elseSuite->firstInBlock())
    return format("(while-else {} {} {})", cond->toString(), suite->toString(),
                  elseSuite->toString());
  else
    return format("(while {} {}})", cond->toString(), suite->toString());
}
StmtPtr WhileStmt::clone() const { return make_unique<WhileStmt>(*this); }
void WhileStmt::accept(ASTVisitor &visitor) const { visitor.visit(this); }

ForStmt::ForStmt(ExprPtr var, ExprPtr iter, StmtPtr suite, StmtPtr elseSuite)
    : var(move(var)), iter(move(iter)), suite(move(suite)), elseSuite(move(elseSuite)) {
}
ForStmt::ForStmt(const ForStmt &stmt)
    : Stmt(stmt), var(ast::clone(stmt.var)), iter(ast::clone(stmt.iter)),
      suite(ast::clone(stmt.suite)), elseSuite(ast::clone(stmt.elseSuite)) {}
string ForStmt::toString() const {
  if (elseSuite && elseSuite->firstInBlock())
    return format("(for-else {} {} {} {})", var->toString(), iter->toString(),
                  suite->toString(), elseSuite->toString());
  else
    return format("(for {} {} {})", var->toString(), iter->toString(),
                  suite->toString());
}
StmtPtr ForStmt::clone() const { return make_unique<ForStmt>(*this); }
void ForStmt::accept(ASTVisitor &visitor) const { visitor.visit(this); }

IfStmt::If IfStmt::If::clone() const { return {ast::clone(cond), ast::clone(suite)}; }

IfStmt::IfStmt(vector<IfStmt::If> &&ifs) : ifs(move(ifs)) {}
IfStmt::IfStmt(ExprPtr cond, StmtPtr suite) {
  ifs.push_back(If{move(cond), move(suite)});
}
IfStmt::IfStmt(ExprPtr cond, StmtPtr suite, ExprPtr elseCond, StmtPtr elseSuite) {
  ifs.push_back(If{move(cond), move(suite)});
  ifs.push_back(If{move(elseCond), move(elseSuite)});
}
IfStmt::IfStmt(const IfStmt &stmt) : Stmt(stmt), ifs(ast::clone_nop(stmt.ifs)) {}
string IfStmt::toString() const {
  string s;
  for (auto &i : ifs)
    s += format(" ({}{})", i.cond ? format("elif {} ", i.cond->toString()) : "else ",
                i.suite->toString());
  return format("(if {})", s);
}
StmtPtr IfStmt::clone() const { return make_unique<IfStmt>(*this); }
void IfStmt::accept(ASTVisitor &visitor) const { visitor.visit(this); }

MatchStmt::MatchStmt(ExprPtr what, vector<PatternPtr> &&patterns,
                     vector<StmtPtr> &&cases)
    : what(move(what)), patterns(move(patterns)), cases(move(cases)) {
  assert(patterns.size() == cases.size());
}
MatchStmt::MatchStmt(ExprPtr what, vector<pair<PatternPtr, StmtPtr>> &&patternCasePairs)
    : what(move(what)) {
  for (auto &i : patternCasePairs) {
    patterns.push_back(move(i.first));
    cases.push_back(move(i.second));
  }
}
MatchStmt::MatchStmt(const MatchStmt &stmt)
    : Stmt(stmt), what(ast::clone(stmt.what)), patterns(ast::clone(stmt.patterns)),
      cases(ast::clone(stmt.cases)) {}
string MatchStmt::toString() const {
  string s;
  for (int i = 0; i < patterns.size(); i++)
    s += format(" (case {} {})", patterns[i]->toString(), cases[i]->toString());
  return format("(match{})", s);
}
StmtPtr MatchStmt::clone() const { return make_unique<MatchStmt>(*this); }
void MatchStmt::accept(ASTVisitor &visitor) const { visitor.visit(this); }

ImportStmt::ImportStmt(ExprPtr from, ExprPtr what, vector<Param> &&args, ExprPtr ret,
                       string as, int dots)
    : from(move(from)), what(move(what)), as(move(as)), dots(dots), args(move(args)),
      ret(move(ret)) {}
ImportStmt::ImportStmt(const ImportStmt &stmt)
    : Stmt(stmt), from(ast::clone(stmt.from)), what(ast::clone(stmt.what)), as(stmt.as),
      dots(stmt.dots), args(ast::clone_nop(stmt.args)), ret(ast::clone(stmt.ret)) {}
string ImportStmt::toString() const {
  return format("(import {}{}{}{})", what->toString(),
                as.empty() ? "" : format(" #:as '{}", as),
                from ? format(" #:from {}", from->toString()) : "",
                dots ? format(" #:dots {}", dots) : "",
                ret ? format(" #:ret {}", ret->toString()) : "");
}
StmtPtr ImportStmt::clone() const { return make_unique<ImportStmt>(*this); }
void ImportStmt::accept(ASTVisitor &visitor) const { visitor.visit(this); }

TryStmt::Catch TryStmt::Catch::clone() const {
  return {var, ast::clone(exc), ast::clone(suite)};
}

TryStmt::TryStmt(StmtPtr suite, vector<Catch> &&catches, StmtPtr finally)
    : suite(move(suite)), catches(move(catches)), finally(move(finally)) {}
TryStmt::TryStmt(const TryStmt &stmt)
    : Stmt(stmt), suite(ast::clone(stmt.suite)), catches(ast::clone_nop(stmt.catches)),
      finally(ast::clone(stmt.finally)) {}
string TryStmt::toString() const {
  string s;
  for (auto &i : catches)
    s += format(" (catch {}{}{})", !i.var.empty() ? format("#:var '{} ", i.var) : "",
                i.exc ? format("#:exc {} ", i.exc->toString()) : "",
                i.suite->toString());
  auto f = format("{}", finally->toString());
  return format("(try {}{}{})", suite->toString(), s,
                !f.empty() ? format(" (finally {})", f) : "");
}
StmtPtr TryStmt::clone() const { return make_unique<TryStmt>(*this); }
void TryStmt::accept(ASTVisitor &visitor) const { visitor.visit(this); }

ThrowStmt::ThrowStmt(ExprPtr expr) : expr(move(expr)) {}
ThrowStmt::ThrowStmt(const ThrowStmt &stmt) : Stmt(stmt), expr(ast::clone(stmt.expr)) {}
string ThrowStmt::toString() const { return format("(throw {})", expr->toString()); }
StmtPtr ThrowStmt::clone() const { return make_unique<ThrowStmt>(*this); }
void ThrowStmt::accept(ASTVisitor &visitor) const { visitor.visit(this); }

GlobalStmt::GlobalStmt(string var) : var(move(var)) {}
string GlobalStmt::toString() const { return format("(global '{})", var); }
StmtPtr GlobalStmt::clone() const { return make_unique<GlobalStmt>(*this); }
void GlobalStmt::accept(ASTVisitor &visitor) const { visitor.visit(this); }

FunctionStmt::FunctionStmt(string name, ExprPtr ret, vector<Param> &&generics,
                           vector<Param> &&args, StmtPtr suite,
                           vector<string> &&attributes)
    : name(move(name)), ret(move(ret)), generics(move(generics)), args(move(args)),
      suite(move(suite)) {
  for (auto &a : attributes)
    this->attributes[a] = "";
}
FunctionStmt::FunctionStmt(string name, ExprPtr ret, vector<Param> &&generics,
                           vector<Param> &&args, StmtPtr suite,
                           map<string, string> &&attributes)
    : name(move(name)), ret(move(ret)), generics(move(generics)), args(move(args)),
      suite(move(suite)), attributes(attributes) {}
FunctionStmt::FunctionStmt(const FunctionStmt &stmt)
    : Stmt(stmt), name(stmt.name), ret(ast::clone(stmt.ret)),
      generics(ast::clone_nop(stmt.generics)), args(ast::clone_nop(stmt.args)),
      suite(ast::clone(stmt.suite)), attributes(stmt.attributes) {}
string FunctionStmt::toString() const {
  string gs;
  for (auto &a : generics)
    gs += " " + a.toString();
  string as;
  for (auto &a : args)
    as += " " + a.toString();
  vector<string> attr;
  for (auto &a : attributes)
    attr.push_back(format("('{}{})", a.first, a.second.empty() ? "" : " '" + a.second));

  return format("(fn '{} ({}){}{} (attr {}) {})", name, as,
                ret ? " #:ret " + ret->toString() : "",
                !generics.empty() ? format(" #:generics ({})", gs) : "",
                join(attr, " "), suite ? suite->toString() : "(pass)");
}
StmtPtr FunctionStmt::clone() const { return make_unique<FunctionStmt>(*this); }
void FunctionStmt::accept(ASTVisitor &visitor) const { visitor.visit(this); }
string FunctionStmt::signature() const {
  vector<string> s;
  for (auto &a : generics)
    s.push_back(a.name);
  for (auto &a : args)
    s.push_back(a.type ? a.type->toString() : "-");
  return format("{}", join(s, ":"));
}

ClassStmt::ClassStmt(string name, vector<Param> &&g, vector<Param> &&a, StmtPtr s,
                     map<string, string> &&at)
    : name(move(name)), generics(move(g)), args(move(a)), suite(move(s)),
      attributes(at) {}
ClassStmt::ClassStmt(string name, vector<Param> &&generics, vector<Param> &&args,
                     StmtPtr suite, vector<string> &&attributes)
    : name(move(name)), generics(move(generics)), args(move(args)), suite(move(suite)) {
  for (auto &a : attributes)
    this->attributes[a] = "";
}
ClassStmt::ClassStmt(const ClassStmt &stmt)
    : Stmt(stmt), name(stmt.name), generics(ast::clone_nop(stmt.generics)),
      args(ast::clone_nop(stmt.args)), suite(ast::clone(stmt.suite)),
      attributes(stmt.attributes) {}
string ClassStmt::toString() const {
  string gs;
  for (auto &a : generics)
    gs += " " + a.toString();
  string as;
  for (auto &a : args)
    as += " " + a.toString();
  vector<string> attr;
  for (auto &a : attributes)
    attr.push_back(format("('{}{})", a.first, a.second.empty() ? "" : " '" + a.second));
  return format("(class '{} ({}){} (attr {}) {})", name, as,
                !generics.empty() ? format(" #:generics ({})", gs) : "",
                join(attr, " "), suite ? suite->toString() : "(pass)");
}
StmtPtr ClassStmt::clone() const { return make_unique<ClassStmt>(*this); }
void ClassStmt::accept(ASTVisitor &visitor) const { visitor.visit(this); }
bool ClassStmt::isRecord() const { return in(attributes, ATTR_TUPLE); }

YieldFromStmt::YieldFromStmt(ExprPtr expr) : expr(move(expr)) {}
YieldFromStmt::YieldFromStmt(const YieldFromStmt &stmt)
    : Stmt(stmt), expr(ast::clone(stmt.expr)) {}
string YieldFromStmt::toString() const {
  return format("(yield-from {})", expr->toString());
}
StmtPtr YieldFromStmt::clone() const { return make_unique<YieldFromStmt>(*this); }
void YieldFromStmt::accept(ASTVisitor &visitor) const { visitor.visit(this); }

WithStmt::WithStmt(vector<ExprPtr> &&items, vector<string> &&vars, StmtPtr suite)
    : items(move(items)), vars(vars), suite(move(suite)) {
  assert(items.size() == vars.size());
}
WithStmt::WithStmt(vector<pair<ExprPtr, string>> &&itemVarPairs, StmtPtr suite)
    : suite(move(suite)) {
  for (auto &i : itemVarPairs) {
    items.push_back(move(i.first));
    vars.push_back(i.second);
  }
}
WithStmt::WithStmt(const WithStmt &stmt)
    : Stmt(stmt), items(ast::clone(stmt.items)), vars(stmt.vars),
      suite(ast::clone(stmt.suite)) {}
string WithStmt::toString() const {
  vector<string> as;
  for (int i = 0; i < items.size(); i++) {
    as.push_back(!vars[i].empty()
                     ? format("({} #:var '{})", items[i]->toString(), vars[i])
                     : items[i]->toString());
  }
  return format("(with ({}) {})", join(as, " "), suite->toString());
}
StmtPtr WithStmt::clone() const { return make_unique<WithStmt>(*this); }
void WithStmt::accept(ASTVisitor &visitor) const { visitor.visit(this); }

AssignMemberStmt::AssignMemberStmt(ExprPtr lhs, string member, ExprPtr rhs)
    : lhs(move(lhs)), member(move(member)), rhs(move(rhs)) {}
AssignMemberStmt::AssignMemberStmt(const AssignMemberStmt &stmt)
    : Stmt(stmt), lhs(ast::clone(stmt.lhs)), member(stmt.member),
      rhs(ast::clone(stmt.rhs)) {}
string AssignMemberStmt::toString() const {
  return format("(assign-member {} {} {})", lhs->toString(), member, rhs->toString());
}
StmtPtr AssignMemberStmt::clone() const { return make_unique<AssignMemberStmt>(*this); }
void AssignMemberStmt::accept(ASTVisitor &visitor) const { visitor.visit(this); }

UpdateStmt::UpdateStmt(ExprPtr lhs, ExprPtr rhs, bool isAtomic)
    : lhs(move(lhs)), rhs(move(rhs)), isAtomic(isAtomic) {}
UpdateStmt::UpdateStmt(const UpdateStmt &stmt)
    : Stmt(stmt), lhs(ast::clone(stmt.lhs)), rhs(ast::clone(stmt.rhs)),
      isAtomic(stmt.isAtomic) {}
string UpdateStmt::toString() const {
  return format("(update {} {})", lhs->toString(), rhs->toString());
}
StmtPtr UpdateStmt::clone() const { return make_unique<UpdateStmt>(*this); }
void UpdateStmt::accept(ASTVisitor &visitor) const { visitor.visit(this); }

} // namespace ast
} // namespace seq
