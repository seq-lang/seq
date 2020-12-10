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
SuiteStmt::SuiteStmt(StmtPtr s, StmtPtr s2, StmtPtr s3, bool o) : ownBlock(o) {
  stmts.push_back(move(s));
  stmts.push_back(move(s2));
  stmts.push_back(move(s3));
}
SuiteStmt::SuiteStmt(const SuiteStmt &s)
    : stmts(ast::clone(s.stmts)), ownBlock(s.ownBlock) {}
string SuiteStmt::toString() const { return format("({})", combine(stmts, "\n  ")); }
StmtPtr SuiteStmt::clone() const { return make_unique<SuiteStmt>(*this); }

PassStmt::PassStmt() {}
PassStmt::PassStmt(const PassStmt &s) {}
string PassStmt::toString() const { return "[PASS]"; }
StmtPtr PassStmt::clone() const { return make_unique<PassStmt>(*this); }

BreakStmt::BreakStmt() {}
BreakStmt::BreakStmt(const BreakStmt &s) {}
string BreakStmt::toString() const { return "[BREAK]"; }
StmtPtr BreakStmt::clone() const { return make_unique<BreakStmt>(*this); }

ContinueStmt::ContinueStmt() {}
ContinueStmt::ContinueStmt(const ContinueStmt &s) {}
string ContinueStmt::toString() const { return "[CONTINUE]"; }
StmtPtr ContinueStmt::clone() const { return make_unique<ContinueStmt>(*this); }

ExprStmt::ExprStmt(ExprPtr e) : expr(move(e)) {}
ExprStmt::ExprStmt(const ExprStmt &s) : expr(ast::clone(s.expr)) {}
string ExprStmt::toString() const { return format("[EXPR {}]", *expr); }
StmtPtr ExprStmt::clone() const { return make_unique<ExprStmt>(*this); }

AssignStmt::AssignStmt(ExprPtr l, ExprPtr r, ExprPtr t, bool m, bool f)
    : lhs(move(l)), rhs(move(r)), type(move(t)), mustExist(m), force(f) {}
AssignStmt::AssignStmt(const AssignStmt &s)
    : lhs(ast::clone(s.lhs)), rhs(ast::clone(s.rhs)), type(ast::clone(s.type)),
      mustExist(s.mustExist), force(s.force) {}
string AssignStmt::toString() const {
  return format("[ASSIGN {} {}{}]", *lhs, *rhs, type ? format(" TYPE={}", *type) : "");
}
StmtPtr AssignStmt::clone() const { return make_unique<AssignStmt>(*this); }

AssignEqStmt::AssignEqStmt(ExprPtr l, ExprPtr r, const string &o)
    : lhs(move(l)), rhs(move(r)), op(o) {}
AssignEqStmt::AssignEqStmt(const AssignEqStmt &s)
    : lhs(ast::clone(s.lhs)), rhs(ast::clone(s.rhs)), op(s.op) {}
string AssignEqStmt::toString() const {
  return format("[ASSIGNEQ {} '{}' {}]", *lhs, op, *rhs);
}
StmtPtr AssignEqStmt::clone() const { return make_unique<AssignEqStmt>(*this); }

DelStmt::DelStmt(ExprPtr e) : expr(move(e)) {}
DelStmt::DelStmt(const DelStmt &s) : expr(ast::clone(s.expr)) {}
string DelStmt::toString() const { return format("[DEL {}]", *expr); }
StmtPtr DelStmt::clone() const { return make_unique<DelStmt>(*this); }

PrintStmt::PrintStmt(ExprPtr e) : expr(move(e)) {}
PrintStmt::PrintStmt(const PrintStmt &s) : expr(ast::clone(s.expr)) {}
string PrintStmt::toString() const { return format("[PRINT {}]", *expr); }
StmtPtr PrintStmt::clone() const { return make_unique<PrintStmt>(*this); }

ReturnStmt::ReturnStmt(ExprPtr e) : expr(move(e)) {}
ReturnStmt::ReturnStmt(const ReturnStmt &s) : expr(ast::clone(s.expr)) {}
string ReturnStmt::toString() const {
  return expr ? format("[RETURN {}]", *expr) : "[RETURN]";
}
StmtPtr ReturnStmt::clone() const { return make_unique<ReturnStmt>(*this); }

YieldStmt::YieldStmt(ExprPtr e) : expr(move(e)) {}
YieldStmt::YieldStmt(const YieldStmt &s) : expr(ast::clone(s.expr)) {}
string YieldStmt::toString() const {
  return expr ? format("[YIELD {}]", *expr) : "[YIELD]";
}
StmtPtr YieldStmt::clone() const { return make_unique<YieldStmt>(*this); }

AssertStmt::AssertStmt(ExprPtr e) : expr(move(e)) {}
AssertStmt::AssertStmt(const AssertStmt &s) : expr(ast::clone(s.expr)) {}
string AssertStmt::toString() const { return format("[ASSERT {}]", *expr); }
StmtPtr AssertStmt::clone() const { return make_unique<AssertStmt>(*this); }

WhileStmt::WhileStmt(ExprPtr c, StmtPtr s, StmtPtr e)
    : cond(move(c)), suite(move(s)), elseSuite(move(e)) {
  if (auto s = CAST(e, SuiteStmt))
    if (!s->stmts.size())
      elseSuite = nullptr;
}
WhileStmt::WhileStmt(const WhileStmt &s)
    : cond(ast::clone(s.cond)), suite(ast::clone(s.suite)),
      elseSuite(ast::clone(s.elseSuite)) {}
string WhileStmt::toString() const {
  return format("[WHILE {} {}{}]", *cond, *suite,
                elseSuite ? format(" ELSE {}", *elseSuite) : "");
}
StmtPtr WhileStmt::clone() const { return make_unique<WhileStmt>(*this); }

ForStmt::ForStmt(ExprPtr v, ExprPtr i, StmtPtr s, StmtPtr e)
    : var(move(v)), iter(move(i)), suite(move(s)), elseSuite(move(e)) {
  if (auto s = CAST(e, SuiteStmt))
    if (!s->stmts.size())
      elseSuite = nullptr;
}
ForStmt::ForStmt(const ForStmt &s)
    : var(ast::clone(s.var)), iter(ast::clone(s.iter)), suite(ast::clone(s.suite)),
      elseSuite(ast::clone(s.elseSuite)), done(ast::clone(s.done)),
      next(ast::clone(s.next)) {}
string ForStmt::toString() const {
  return format("[FOR {} {} {}{}]", *var, *iter, *suite,
                elseSuite ? format(" ELSE {}", *elseSuite) : "");
}
StmtPtr ForStmt::clone() const { return make_unique<ForStmt>(*this); }

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
StmtPtr IfStmt::clone() const { return make_unique<IfStmt>(*this); }

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
StmtPtr MatchStmt::clone() const { return make_unique<MatchStmt>(*this); }

ImportStmt::ImportStmt(ExprPtr f, ExprPtr w, vector<Param> &&p, ExprPtr r, string a,
                       int d)
    : from(move(f)), what(move(w)), args(move(p)), ret(move(r)), as(move(a)), dots(d) {}
ImportStmt::ImportStmt(const ImportStmt &s)
    : from(ast::clone(s.from)), what(ast::clone(s.what)), args(ast::clone_nop(s.args)),
      ret(ast::clone(s.ret)), as(s.as), dots(s.dots) {}
string ImportStmt::toString() const {
  return format("[IMPORT {}{}{}]", *what, as.empty() ? "" : format(" AS={}", as),
                from ? format(" FROM={}", *from) : "");
}
StmtPtr ImportStmt::clone() const { return make_unique<ImportStmt>(*this); }

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
StmtPtr TryStmt::clone() const { return make_unique<TryStmt>(*this); }

GlobalStmt::GlobalStmt(string v) : var(v) {}
GlobalStmt::GlobalStmt(const GlobalStmt &s) : var(s.var) {}
string GlobalStmt::toString() const { return format("[GLOBAL {}]", var); }
StmtPtr GlobalStmt::clone() const { return make_unique<GlobalStmt>(*this); }

ThrowStmt::ThrowStmt(ExprPtr e) : expr(move(e)) {}
ThrowStmt::ThrowStmt(const ThrowStmt &s) : expr(ast::clone(s.expr)) {}
string ThrowStmt::toString() const { return format("[THROW {}]", *expr); }
StmtPtr ThrowStmt::clone() const { return make_unique<ThrowStmt>(*this); }

FunctionStmt::FunctionStmt(string n, ExprPtr r, vector<Param> &&g, vector<Param> &&a,
                           StmtPtr s, vector<string> &&at)
    : name(n), ret(move(r)), generics(move(g)), args(move(a)), suite(move(s)) {
  for (auto &a : at)
    attributes[a] = "";
}
FunctionStmt::FunctionStmt(string n, ExprPtr r, vector<Param> &&g, vector<Param> &&a,
                           StmtPtr s, map<string, string> &&at)
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
StmtPtr FunctionStmt::clone() const { return make_unique<FunctionStmt>(*this); }
string FunctionStmt::signature() const {
  vector<string> s;
  for (auto &a : generics)
    s.push_back(a.name);
  for (auto &a : args)
    s.push_back(a.type ? a.type->toString() : "-");
  return format("{}", join(s, ":"));
}

ClassStmt::ClassStmt(bool i, string n, vector<Param> &&g, vector<Param> &&a, StmtPtr s,
                     map<string, string> &&at)
    : isRecord(i), name(n), generics(move(g)), args(move(a)), suite(move(s)),
      attributes(at) {}
ClassStmt::ClassStmt(bool i, string n, vector<Param> &&g, vector<Param> &&a, StmtPtr s,
                     vector<string> &&at)
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
StmtPtr ClassStmt::clone() const { return make_unique<ClassStmt>(*this); }

YieldFromStmt::YieldFromStmt(ExprPtr e) : expr(move(e)) {}
YieldFromStmt::YieldFromStmt(const YieldFromStmt &s) : expr(ast::clone(s.expr)) {}
string YieldFromStmt::toString() const { return format("[YIELD_FROM {}]", *expr); }
StmtPtr YieldFromStmt::clone() const { return make_unique<YieldFromStmt>(*this); }

WithStmt::WithStmt(vector<ExprPtr> &&i, vector<string> &&v, StmtPtr s)
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
StmtPtr WithStmt::clone() const { return make_unique<WithStmt>(*this); }

AssignMemberStmt::AssignMemberStmt(ExprPtr l, string m, ExprPtr r)
    : lhs(move(l)), member(m), rhs(move(r)) {}
AssignMemberStmt::AssignMemberStmt(const AssignMemberStmt &s)
    : lhs(ast::clone(s.lhs)), member(s.member), rhs(ast::clone(s.rhs)) {}
string AssignMemberStmt::toString() const {
  return format("[ASSIGN_MEMBER {} {} {}]", *lhs, member, *rhs);
}
StmtPtr AssignMemberStmt::clone() const { return make_unique<AssignMemberStmt>(*this); }

UpdateStmt::UpdateStmt(ExprPtr l, ExprPtr r) : lhs(move(l)), rhs(move(r)) {}
UpdateStmt::UpdateStmt(const UpdateStmt &s)
    : lhs(ast::clone(s.lhs)), rhs(ast::clone(s.rhs)) {}
string UpdateStmt::toString() const { return format("[UPDATE {} {}]", *lhs, *rhs); }
StmtPtr UpdateStmt::clone() const { return make_unique<UpdateStmt>(*this); }

} // namespace ast
} // namespace seq
