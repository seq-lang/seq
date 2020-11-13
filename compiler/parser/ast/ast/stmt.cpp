#include <memory>
#include <string>
#include <vector>

#include "parser/ast/ast/stmt.h"

using fmt::format;
using std::move;

namespace seq {
namespace ast {

Stmt::Stmt(const seq::SrcInfo &s) { setSrcInfo(s); }
Stmt::~Stmt() {}

SuiteStmt::SuiteStmt(vector<StmtPtr> &&s, bool o) : stmts(move(s)), ownBlock(o) {}
SuiteStmt::SuiteStmt(StmtPtr s, bool o) : ownBlock(o) { stmts.push_back(move(s)); }
SuiteStmt::SuiteStmt(StmtPtr s, StmtPtr s2, bool o) : ownBlock(o) {
  stmts.push_back(move(s));
  stmts.push_back(move(s2));
}

SuiteStmt::SuiteStmt(const SuiteStmt &s)
    : stmts(ast::clone(s.stmts)), ownBlock(s.ownBlock) {}
string SuiteStmt::toString() const { return format("({})", combine(stmts, "\n  ")); }

PassStmt::PassStmt() {}
PassStmt::PassStmt(const PassStmt &s) {}
string PassStmt::toString() const { return "[PASS]"; }

BreakStmt::BreakStmt() {}
BreakStmt::BreakStmt(const BreakStmt &s) {}
string BreakStmt::toString() const { return "[BREAK]"; }

ContinueStmt::ContinueStmt() {}
ContinueStmt::ContinueStmt(const ContinueStmt &s) {}
string ContinueStmt::toString() const { return "[CONTINUE]"; }

ExprStmt::ExprStmt(ExprPtr e) : expr(move(e)) {}
ExprStmt::ExprStmt(const ExprStmt &s) : expr(ast::clone(s.expr)) {}
string ExprStmt::toString() const { return format("[EXPR {}]", *expr); }

AssignStmt::AssignStmt(ExprPtr l, ExprPtr r, ExprPtr t, bool m, bool f)
    : lhs(move(l)), rhs(move(r)), type(move(t)), mustExist(m), force(f) {}
AssignStmt::AssignStmt(const AssignStmt &s)
    : lhs(ast::clone(s.lhs)), rhs(ast::clone(s.rhs)), type(ast::clone(s.type)),
      mustExist(s.mustExist), force(s.force) {}
string AssignStmt::toString() const {
  return format("[ASSIGN {} {}{}]", *lhs, *rhs, type ? format(" TYPE={}", *type) : "");
}

AssignEqStmt::AssignEqStmt(ExprPtr l, ExprPtr r, const string &o)
    : lhs(move(l)), rhs(move(r)), op(o) {}
AssignEqStmt::AssignEqStmt(const AssignEqStmt &s)
    : lhs(ast::clone(s.lhs)), rhs(ast::clone(s.rhs)), op(s.op) {}
string AssignEqStmt::toString() const {
  return format("[ASSIGNEQ {} '{}' {}]", *lhs, op, *rhs);
}

DelStmt::DelStmt(ExprPtr e) : expr(move(e)) {}
DelStmt::DelStmt(const DelStmt &s) : expr(ast::clone(s.expr)) {}
string DelStmt::toString() const { return format("[DEL {}]", *expr); }

PrintStmt::PrintStmt(ExprPtr e) : expr(move(e)) {}
PrintStmt::PrintStmt(const PrintStmt &s) : expr(ast::clone(s.expr)) {}
string PrintStmt::toString() const { return format("[PRINT {}]", *expr); }

ReturnStmt::ReturnStmt(ExprPtr e) : expr(move(e)) {}
ReturnStmt::ReturnStmt(const ReturnStmt &s) : expr(ast::clone(s.expr)) {}
string ReturnStmt::toString() const {
  return expr ? format("[RETURN {}]", *expr) : "[RETURN]";
}

YieldStmt::YieldStmt(ExprPtr e) : expr(move(e)) {}
YieldStmt::YieldStmt(const YieldStmt &s) : expr(ast::clone(s.expr)) {}
string YieldStmt::toString() const {
  return expr ? format("[YIELD {}]", *expr) : "[YIELD]";
}

AssertStmt::AssertStmt(ExprPtr e) : expr(move(e)) {}
AssertStmt::AssertStmt(const AssertStmt &s) : expr(ast::clone(s.expr)) {}
string AssertStmt::toString() const { return format("[ASSERT {}]", *expr); }

WhileStmt::WhileStmt(ExprPtr c, StmtPtr s, StmtPtr e)
    : cond(move(c)), suite(move(s)), elseSuite(move(e)) {}
WhileStmt::WhileStmt(const WhileStmt &s)
    : cond(ast::clone(s.cond)), suite(ast::clone(s.suite)),
      elseSuite(ast::clone(s.suite)) {}
string WhileStmt::toString() const {
  return format("[WHILE {} {}{}]", *cond, *suite,
                elseSuite ? format(" ELSE {}", *elseSuite) : "");
}

ForStmt::ForStmt(ExprPtr v, ExprPtr i, StmtPtr s, StmtPtr e)
    : var(move(v)), iter(move(i)), suite(move(s)), elseSuite(move(e)) {}
ForStmt::ForStmt(const ForStmt &s)
    : var(ast::clone(s.var)), iter(ast::clone(s.iter)), suite(ast::clone(s.suite)),
      elseSuite(ast::clone(s.suite)) {}
string ForStmt::toString() const {
  return format("[FOR {} {} {}{}]", *var, *iter, *suite,
                elseSuite ? format(" ELSE {}", *elseSuite) : "");
}

IfStmt::If IfStmt::If::clone() const { return {ast::clone(cond), ast::clone(suite)}; }

IfStmt::IfStmt(vector<IfStmt::If> &&i) : ifs(move(i)) {}
IfStmt::IfStmt(ExprPtr cond, StmtPtr suite) {
  ifs.push_back(If{move(cond), move(suite)});
}
IfStmt::IfStmt(ExprPtr cond, StmtPtr suite, ExprPtr econd, StmtPtr esuite) {
  ifs.push_back(If{move(cond), move(suite)});
  ifs.push_back(If{move(econd), move(esuite)});
}
IfStmt::IfStmt(const IfStmt &s) : ifs(ast::clone_nop(s.ifs)) {}
string IfStmt::toString() const {
  string s;
  for (auto &i : ifs)
    s += format(" [{}{}]", i.cond ? format("COND={} ", *i.cond) : "", *i.suite);
  return format("[IF{}]", s);
}

MatchStmt::MatchStmt(ExprPtr w, vector<PatternPtr> &&p, vector<StmtPtr> &&c)
    : what(move(w)), patterns(move(p)), cases(move(c)) {
  assert(p.size() == c.size());
}
MatchStmt::MatchStmt(ExprPtr w, vector<pair<PatternPtr, StmtPtr>> &&v) : what(move(w)) {
  for (auto &i : v) {
    patterns.push_back(move(i.first));
    cases.push_back(move(i.second));
  }
}
MatchStmt::MatchStmt(const MatchStmt &s)
    : what(ast::clone(s.what)), patterns(ast::clone(s.patterns)),
      cases(ast::clone(s.cases)) {}
string MatchStmt::toString() const {
  string s;
  for (int i = 0; i < patterns.size(); i++)
    s += format(" [{} {}]", *patterns[i], *cases[i]);
  return format("[MATCH{}]", s);
}

ImportStmt::ImportStmt(ExprPtr f, ExprPtr w, string a) : from(f), what(w), as(a) {}
ImportStmt::ImportStmt(const ImportStmt &s) : from(s.from), what(s.what) {}
string ImportStmt::toString() const {
  vector<string> s;
  for (auto &w : what) {
    s.push_back(w.second.size() ? format("({} :as {})", w.first, w.second) : w.first);
  }
  return format("[IMPORT {}{})",
                from.second.size() ? format("({} :as {})", from.first, from.second)
                                   : from.first,
                s.size() ? format(" :what {}", fmt::join(s, " ")) : "");
}

TryStmt::Catch TryStmt::Catch::clone() const {
  return {var, ast::clone(exc), ast::clone(suite)};
}

TryStmt::TryStmt(StmtPtr s, vector<Catch> &&c, StmtPtr f)
    : suite(move(s)), catches(move(c)), finally(move(f)) {}
TryStmt::TryStmt(const TryStmt &s)
    : suite(ast::clone(s.suite)), catches(ast::clone_nop(s.catches)),
      finally(ast::clone(s.finally)) {}
string TryStmt::toString() const {
  string s;
  for (auto &i : catches)
    s += format(" [{}{}{}]", i.var != "" ? format("VAR={} ", i.var) : "",
                i.exc ? format("EXC={} ", *i.exc) : "", *i.suite);
  auto f = format("{}", *finally);
  return format("[TRY {}{}{}]", *suite, s, f.size() ? format(" [FINALLY {}]", f) : "");
}

GlobalStmt::GlobalStmt(const string &v) : var(v) {}
GlobalStmt::GlobalStmt(const GlobalStmt &s) : var(s.var) {}
string GlobalStmt::toString() const { return format("[GLOBAL {}]", var); }

ThrowStmt::ThrowStmt(ExprPtr e) : expr(move(e)) {}
ThrowStmt::ThrowStmt(const ThrowStmt &s) : expr(ast::clone(s.expr)) {}
string ThrowStmt::toString() const { return format("[THROW {}]", *expr); }

FunctionStmt::FunctionStmt(const string &n, ExprPtr r, vector<Param> &&g,
                           vector<Param> &&a, StmtPtr s, const vector<string> &at)
    : name(n), ret(move(r)), generics(move(g)), args(move(a)), suite(move(s)) {
  for (auto &a : at)
    attributes[a] = "";
}
FunctionStmt::FunctionStmt(const string &n, ExprPtr r, vector<Param> &&g,
                           vector<Param> &&a, StmtPtr s, const map<string, string> &at)
    : name(n), ret(move(r)), generics(move(g)), args(move(a)), suite(move(s)),
      attributes(at) {}
FunctionStmt::FunctionStmt(const FunctionStmt &s)
    : name(s.name), ret(ast::clone(s.ret)), generics(ast::clone_nop(s.generics)),
      args(ast::clone_nop(s.args)), suite(ast::clone(s.suite)),
      attributes(s.attributes) {}
string FunctionStmt::toString() const {
  string gs;
  for (auto &a : generics)
    gs += " " + a.toString();
  string as;
  for (auto &a : args)
    as += " " + a.toString();
  return format(
      "[DEF {}{}{}{} {}]", name, ret ? " :ret " + ret->toString() : "",
      generics.size() ? format(" GEN={}", gs) : "", args.size() ? " ARGS=" + as : "",
      // attributes.size() ? format(" :attrs ({})", fmt::join(attributes, " ")) : "",
      suite ? suite->toString() : "[PASS]");
}
string FunctionStmt::signature() const {
  vector<string> s;
  for (auto &a : generics)
    s.push_back(a.name);
  for (auto &a : args)
    s.push_back(a.type ? a.type->toString() : "-");
  return format("{}", join(s, ":"));
}

ClassStmt::ClassStmt(bool i, const string &n, vector<Param> &&g, vector<Param> &&a,
                     StmtPtr s, const map<string, string> &at)
    : isRecord(i), name(n), generics(move(g)), args(move(a)), suite(move(s)),
      attributes(at) {}
ClassStmt::ClassStmt(bool i, const string &n, vector<Param> &&g, vector<Param> &&a,
                     StmtPtr s, const vector<string> &at)
    : isRecord(i), name(n), generics(move(g)), args(move(a)), suite(move(s)) {
  for (auto &a : at)
    attributes[a] = "";
}
ClassStmt::ClassStmt(const ClassStmt &s)
    : isRecord(s.isRecord), name(s.name), generics(ast::clone_nop(s.generics)),
      args(ast::clone_nop(s.args)), suite(ast::clone(s.suite)),
      attributes(s.attributes) {}
string ClassStmt::toString() const {
  string gs;
  for (auto &a : generics)
    gs += " " + a.toString();
  string as;
  for (auto &a : args)
    as += " " + a.toString();
  return format(
      "[{} {}{}{} {}]", (isRecord ? "CLASS" : "TYPE"), name,
      generics.size() ? format(" GEN={}", gs) : "", args.size() ? " ARGS=" + as : "",
      // attributes.size() ? format(" :attrs ({})", fmt::join(attributes, " ")) : "",
      *suite);
}

YieldFromStmt::YieldFromStmt(ExprPtr e) : expr(move(e)) {}
YieldFromStmt::YieldFromStmt(const YieldFromStmt &s) : expr(ast::clone(s.expr)) {}
string YieldFromStmt::toString() const { return format("[YIELD_FROM {}]", *expr); }

WithStmt::WithStmt(vector<ExprPtr> &&i, const vector<string> &v, StmtPtr s)
    : items(move(i)), vars(v), suite(move(s)) {
  assert(i.size() == v.size());
}
WithStmt::WithStmt(vector<pair<ExprPtr, string>> &&v, StmtPtr s) : suite(move(s)) {
  for (auto &i : v) {
    items.push_back(move(i.first));
    vars.push_back(i.second);
  }
}
WithStmt::WithStmt(const WithStmt &s)
    : items(ast::clone(s.items)), vars(s.vars), suite(ast::clone(s.suite)) {}
string WithStmt::toString() const {
  vector<string> as;
  for (int i = 0; i < items.size(); i++) {
    as.push_back(vars[i].size() ? format("({} VAR={})", *items[i], vars[i])
                                : items[i]->toString());
  }
  return format("[WITH ({}) {}]", fmt::join(as, " "), *suite);
}

AssignMemberStmt::AssignMemberStmt(ExprPtr l, const string &m, ExprPtr r)
    : lhs(move(l)), member(m), rhs(move(r)) {}
AssignMemberStmt::AssignMemberStmt(const AssignMemberStmt &s)
    : lhs(ast::clone(s.lhs)), member(s.member), rhs(ast::clone(s.rhs)) {}
string AssignMemberStmt::toString() const {
  return format("[ASSIGN_MEMBER {} {} {}]", *lhs, member, *rhs);
}

UpdateStmt::UpdateStmt(ExprPtr l, ExprPtr r) : lhs(move(l)), rhs(move(r)) {}
UpdateStmt::UpdateStmt(const UpdateStmt &s)
    : lhs(ast::clone(s.lhs)), rhs(ast::clone(s.rhs)) {}
string UpdateStmt::toString() const { return format("[UPDATE {} {}]", *lhs, *rhs); }

} // namespace ast
} // namespace seq
