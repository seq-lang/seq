#include <memory>
#include <string>
#include <vector>

#include "parser/ast/ast.h"

using fmt::format;
using std::move;
using std::pair;
using std::string;
using std::unique_ptr;
using std::vector;

namespace seq {
namespace ast {

string Expr::wrap(const string &s) const {
  return format("({}{})", s, _type ? format(" :type {}", _type->toString()) : "");
}

Expr::Expr() : _type(nullptr), _isType(false) {}
Expr::Expr(const Expr &e) : seq::SrcObject(e), _type(e._type), _isType(e._isType) {}

NoneExpr::NoneExpr() : Expr() {}
NoneExpr::NoneExpr(const NoneExpr &e) : Expr(e) {}
string NoneExpr::toString() const { return wrap("#none"); }

BoolExpr::BoolExpr(bool v) : Expr(), value(v) {}
BoolExpr::BoolExpr(const BoolExpr &e) : Expr(e), value(e.value) {}
string BoolExpr::toString() const { return wrap(format("#bool {}", int(value))); }

IntExpr::IntExpr(int v)
    : Expr(), value(std::to_string(v)), suffix(""), intValue(v), sign(false) {}
IntExpr::IntExpr(const IntExpr &e)
    : Expr(e), value(e.value), suffix(e.suffix), intValue(e.intValue), sign(e.sign) {}
IntExpr::IntExpr(const string &v, const string &s)
    : Expr(), value(v), suffix(s), intValue(0), sign(false) {}
string IntExpr::toString() const {
  return wrap(format("#int {}{}", value, suffix == "" ? "" : format(" {}", suffix)));
}

FloatExpr::FloatExpr(double v, const string &s) : Expr(), value(v), suffix(s) {}
FloatExpr::FloatExpr(const FloatExpr &e) : Expr(e), value(e.value), suffix(e.suffix) {}
string FloatExpr::toString() const {
  return wrap(format("#float {}{}", value, suffix == "" ? "" : format(" {}", suffix)));
}

StringExpr::StringExpr(const string &v) : Expr(), value(v) {}
StringExpr::StringExpr(const StringExpr &e) : Expr(e), value(e.value) {}
string StringExpr::toString() const { return wrap(format("#str '{}'", escape(value))); }

FStringExpr::FStringExpr(const string &v) : Expr(), value(v) {}
FStringExpr::FStringExpr(const FStringExpr &e) : Expr(e), value(e.value) {}
string FStringExpr::toString() const {
  return wrap(format("#fstr '{}'", escape(value)));
}

KmerExpr::KmerExpr(const string &v) : Expr(), value(v) {}
KmerExpr::KmerExpr(const KmerExpr &e) : Expr(e), value(e.value) {}
string KmerExpr::toString() const { return wrap(format("#kmer '{}'", escape(value))); }

SeqExpr::SeqExpr(const string &v, const string &p) : Expr(), prefix(p), value(v) {}
SeqExpr::SeqExpr(const SeqExpr &e) : Expr(e), prefix(e.prefix), value(e.value) {}
string SeqExpr::toString() const {
  return wrap(format("#seq '{}'{}", value, prefix == "" ? "" : format(" {}", prefix)));
}

IdExpr::IdExpr(const string &v) : Expr(), value(v) {}
IdExpr::IdExpr(const IdExpr &e) : Expr(e), value(e.value) {}
string IdExpr::toString() const { return wrap(format("#id {}", value)); }

UnpackExpr::UnpackExpr(ExprPtr v) : Expr(), what(move(v)) {}
UnpackExpr::UnpackExpr(const UnpackExpr &e) : Expr(e), what(ast::clone(e.what)) {}
string UnpackExpr::toString() const { return wrap(format("#unpack {}", *what)); }

TupleExpr::TupleExpr(vector<ExprPtr> &&i) : Expr(), items(move(i)) {}
TupleExpr::TupleExpr(const TupleExpr &e) : Expr(e), items(ast::clone(e.items)) {}
string TupleExpr::toString() const { return wrap(format("#tuple {}", combine(items))); }

ListExpr::ListExpr(vector<ExprPtr> &&i) : Expr(), items(move(i)) {}
ListExpr::ListExpr(const ListExpr &e) : Expr(e), items(ast::clone(e.items)) {}
string ListExpr::toString() const {
  return wrap(items.size() ? format("#list {}", combine(items)) : "#list");
}

SetExpr::SetExpr(vector<ExprPtr> &&i) : Expr(), items(move(i)) {}
SetExpr::SetExpr(const SetExpr &e) : Expr(e), items(ast::clone(e.items)) {}
string SetExpr::toString() const {
  return wrap(items.size() ? format("#set {}", combine(items)) : "#set");
}

DictExpr::KeyValue DictExpr::KeyValue::clone() const {
  return {ast::clone(key), ast::clone(value)};
}

DictExpr::DictExpr(vector<DictExpr::KeyValue> &&i) : Expr(), items(move(i)) {}
DictExpr::DictExpr(const DictExpr &e) : Expr(e), items(ast::clone_nop(e.items)) {}
string DictExpr::toString() const {
  vector<string> s;
  for (auto &i : items)
    s.push_back(format("{} {}", *i.key, *i.value));
  return wrap(s.size() ? format("#dict {}", fmt::join(s, " ")) : "#dict");
}

GeneratorExpr::Body GeneratorExpr::Body::clone() const {
  return {ast::clone(vars), ast::clone(gen), ast::clone(conds)};
}

GeneratorExpr::GeneratorExpr(GeneratorExpr::Kind k, ExprPtr e,
                             vector<GeneratorExpr::Body> &&l)
    : Expr(), kind(k), expr(move(e)), loops(move(l)) {}
GeneratorExpr::GeneratorExpr(const GeneratorExpr &e)
    : Expr(e), kind(e.kind), expr(ast::clone(e.expr)), loops(ast::clone_nop(e.loops)) {}
string GeneratorExpr::toString() const {
  string prefix = "";
  if (kind == Kind::ListGenerator)
    prefix = "list_";
  if (kind == Kind::SetGenerator)
    prefix = "set_";
  string s;
  for (auto &i : loops) {
    string q;
    for (auto &k : i.conds)
      q += format(" #if {}", *k);
    s += format("#for {} {}{}", i.vars->toString(), i.gen->toString(), q);
  }
  return wrap(format("#{}gen {}{}", prefix, *expr, s));
}

DictGeneratorExpr::DictGeneratorExpr(ExprPtr k, ExprPtr e,
                                     vector<GeneratorExpr::Body> &&l)
    : Expr(), key(move(k)), expr(move(e)), loops(move(l)) {}
DictGeneratorExpr::DictGeneratorExpr(const DictGeneratorExpr &e)
    : Expr(e), key(ast::clone(e.key)), expr(ast::clone(e.expr)),
      loops(ast::clone_nop(e.loops)) {}
string DictGeneratorExpr::toString() const {
  string s;
  for (auto &i : loops) {
    string q;
    for (auto &k : i.conds)
      q += format(" #if {}", *k);
    s += format("#for ({}) {}{}", i.vars->toString(), i.gen->toString(), q);
  }
  return wrap(format("#dict_gen {} {}{}", *key, *expr, s));
}

IfExpr::IfExpr(ExprPtr c, ExprPtr i, ExprPtr e)
    : Expr(), cond(move(c)), eif(move(i)), eelse(move(e)) {}
IfExpr::IfExpr(const IfExpr &e)
    : Expr(e), cond(ast::clone(e.cond)), eif(ast::clone(e.eif)),
      eelse(ast::clone(e.eelse)) {}
string IfExpr::toString() const {
  return wrap(format("#if {} {} {}", *cond, *eif, *eelse));
}

UnaryExpr::UnaryExpr(const string &o, ExprPtr e) : Expr(), op(o), expr(move(e)) {}
UnaryExpr::UnaryExpr(const UnaryExpr &e)
    : Expr(e), op(e.op), expr(ast::clone(e.expr)) {}
string UnaryExpr::toString() const { return wrap(format("#unary '{}' {}", op, *expr)); }

BinaryExpr::BinaryExpr(ExprPtr l, const string &o, ExprPtr r, bool i)
    : Expr(), op(o), lexpr(move(l)), rexpr(move(r)), inPlace(i) {}
BinaryExpr::BinaryExpr(const BinaryExpr &e)
    : Expr(e), op(e.op), lexpr(ast::clone(e.lexpr)), rexpr(ast::clone(e.rexpr)),
      inPlace(e.inPlace) {}
string BinaryExpr::toString() const {
  return wrap(
      format("#binary {} '{}' {}{}", *lexpr, op, *rexpr, inPlace ? " :inplace" : ""));
}

PipeExpr::Pipe PipeExpr::Pipe::clone() const { return {op, ast::clone(expr)}; }

PipeExpr::PipeExpr(vector<PipeExpr::Pipe> &&i) : Expr(), items(move(i)) {}
PipeExpr::PipeExpr(const PipeExpr &e)
    : Expr(e), items(ast::clone_nop(e.items)), inTypes(e.inTypes) {}
string PipeExpr::toString() const {
  vector<string> s;
  for (auto &i : items)
    s.push_back(format("({}{})", *i.expr, i.op == "||>" ? " :parallel" : ""));
  return wrap(format("#pipe {}", fmt::join(s, " ")));
}

IndexExpr::IndexExpr(ExprPtr e, ExprPtr i) : Expr(), expr(move(e)), index(move(i)) {}
IndexExpr::IndexExpr(const IndexExpr &e)
    : Expr(e), expr(ast::clone(e.expr)), index(ast::clone(e.index)) {}
string IndexExpr::toString() const {
  return wrap(format("#index {} {}", *expr, *index));
}

TupleIndexExpr::TupleIndexExpr(ExprPtr e, int i) : Expr(), expr(move(e)), index(i) {}
TupleIndexExpr::TupleIndexExpr(const TupleIndexExpr &e)
    : Expr(e), expr(ast::clone(e.expr)), index(e.index) {}
string TupleIndexExpr::toString() const {
  return wrap(format("#tupleindex {} {}", *expr, index));
}

StackAllocExpr::StackAllocExpr(ExprPtr t, ExprPtr e)
    : Expr(), typeExpr(move(t)), expr(move(e)) {}
StackAllocExpr::StackAllocExpr(const StackAllocExpr &e)
    : Expr(e), typeExpr(ast::clone(e.typeExpr)), expr(ast::clone(e.expr)) {}
string StackAllocExpr::toString() const {
  return wrap(format("#alloca {} {}", *typeExpr, *expr));
}

CallExpr::Arg CallExpr::Arg::clone() const { return {name, ast::clone(value)}; }
CallExpr::CallExpr(const CallExpr &e)
    : Expr(e), expr(ast::clone(e.expr)), args(ast::clone_nop(e.args)) {}
CallExpr::CallExpr(ExprPtr e, vector<CallExpr::Arg> &&a)
    : Expr(), expr(move(e)), args(move(a)) {}
CallExpr::CallExpr(ExprPtr e, vector<ExprPtr> &&arg) : Expr(), expr(move(e)) {
  for (auto &i : arg) {
    args.push_back(CallExpr::Arg{"", move(i)});
  }
}
CallExpr::CallExpr(ExprPtr e, ExprPtr arg, ExprPtr arg2, ExprPtr arg3)
    : Expr(), expr(move(e)) {
  if (arg)
    args.push_back(CallExpr::Arg{"", move(arg)});
  if (arg2)
    args.push_back(CallExpr::Arg{"", move(arg2)});
  if (arg3)
    args.push_back(CallExpr::Arg{"", move(arg3)});
}
string CallExpr::toString() const {
  string s;
  for (auto &i : args)
    if (i.name == "") {
      s += " " + i.value->toString();
    } else {
      s += format(" ({} :name {})", *i.value, i.name);
    }
  return wrap(format("#call {}{}", *expr, s));
}

DotExpr::DotExpr(ExprPtr e, const string &m) : Expr(), expr(move(e)), member(m) {}
DotExpr::DotExpr(const DotExpr &e)
    : Expr(e), expr(ast::clone(e.expr)), member(e.member) {}
string DotExpr::toString() const { return wrap(format("#dot {} {}", *expr, member)); }

SliceExpr::SliceExpr(ExprPtr s, ExprPtr e, ExprPtr st)
    : Expr(), st(move(s)), ed(move(e)), step(move(st)) {}
SliceExpr::SliceExpr(const SliceExpr &e)
    : Expr(e), st(ast::clone(e.st)), ed(ast::clone(e.ed)), step(ast::clone(e.step)) {}
string SliceExpr::toString() const {
  return wrap(format("#slice{}{}{}", st ? format(" :start {}", *st) : "",
                     ed ? format(" :end {}", *ed) : "",
                     step ? format(" :step {}", *step) : ""));
}

EllipsisExpr::EllipsisExpr() : Expr() {}
EllipsisExpr::EllipsisExpr(const EllipsisExpr &e) : Expr(e) {}
string EllipsisExpr::toString() const { return wrap("#ellipsis"); }

TypeOfExpr::TypeOfExpr(ExprPtr e) : Expr(), expr(move(e)) {}
TypeOfExpr::TypeOfExpr(const TypeOfExpr &e) : Expr(e), expr(ast::clone(e.expr)) {}
string TypeOfExpr::toString() const { return wrap(format("#typeof {}", *expr)); }

PtrExpr::PtrExpr(ExprPtr e) : Expr(), expr(move(e)) {}
PtrExpr::PtrExpr(const PtrExpr &e) : Expr(e), expr(ast::clone(e.expr)) {}
string PtrExpr::toString() const { return wrap(format("#ptr {}", *expr)); }

LambdaExpr::LambdaExpr(vector<string> v, ExprPtr e) : Expr(), vars(v), expr(move(e)) {}
LambdaExpr::LambdaExpr(const LambdaExpr &e)
    : Expr(e), vars(e.vars), expr(ast::clone(e.expr)) {}
string LambdaExpr::toString() const {
  return wrap(format("#lambda {} {}", fmt::join(vars, " "), *expr));
}

YieldExpr::YieldExpr() : Expr() {}
YieldExpr::YieldExpr(const YieldExpr &e) : Expr(e) {}
string YieldExpr::toString() const { return "#yield"; }

InstantiateExpr::InstantiateExpr(ExprPtr e, vector<ExprPtr> &&i)
    : Expr(), type(move(e)), params(move(i)) {}
InstantiateExpr::InstantiateExpr(const InstantiateExpr &e)
    : Expr(e), type(ast::clone(e.type)), params(ast::clone(e.params)) {}
string InstantiateExpr::toString() const {
  return wrap(format("#instantiate {} {}", *type, combine(params)));
}

StaticExpr::StaticExpr(ExprPtr e, const std::set<std::string> &c)
    : Expr(), expr(move(e)), captures(c) {}
StaticExpr::StaticExpr(const StaticExpr &e)
    : Expr(e), expr(ast::clone(e.expr)), captures(e.captures) {}
string StaticExpr::toString() const { return wrap(format("#static {}", *expr)); }

Param Param::clone() const { return {name, ast::clone(type), ast::clone(deflt)}; }
string Param::toString() const {
  return format("({}{}{})", name, type ? " :typ " + type->toString() : "",
                deflt ? " :default " + deflt->toString() : "");
}

Stmt::Stmt(const seq::SrcInfo &s) { setSrcInfo(s); }

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
string PassStmt::toString() const { return "#pass"; }

BreakStmt::BreakStmt() {}
BreakStmt::BreakStmt(const BreakStmt &s) {}
string BreakStmt::toString() const { return "#break"; }

ContinueStmt::ContinueStmt() {}
ContinueStmt::ContinueStmt(const ContinueStmt &s) {}
string ContinueStmt::toString() const { return "#continue"; }

ExprStmt::ExprStmt(ExprPtr e) : expr(move(e)) {}
ExprStmt::ExprStmt(const ExprStmt &s) : expr(ast::clone(s.expr)) {}
string ExprStmt::toString() const { return format("(#expr {})", *expr); }

AssignStmt::AssignStmt(ExprPtr l, ExprPtr r, ExprPtr t, bool m, bool f)
    : lhs(move(l)), rhs(move(r)), type(move(t)), mustExist(m), force(f) {}
AssignStmt::AssignStmt(const AssignStmt &s)
    : lhs(ast::clone(s.lhs)), rhs(ast::clone(s.rhs)), type(ast::clone(s.type)),
      mustExist(s.mustExist), force(s.force) {}
string AssignStmt::toString() const {
  return format("(#assign {} {}{})", *lhs, *rhs,
                type ? format(" :type {}", *type) : "");
}

AssignMemberStmt::AssignMemberStmt(ExprPtr l, const string &m, ExprPtr r)
    : lhs(move(l)), member(m), rhs(move(r)) {}
AssignMemberStmt::AssignMemberStmt(const AssignMemberStmt &s)
    : lhs(ast::clone(s.lhs)), member(s.member), rhs(ast::clone(s.rhs)) {}
string AssignMemberStmt::toString() const {
  return format("(#assignmember {} {} {})", *lhs, member, *rhs);
}

UpdateStmt::UpdateStmt(ExprPtr l, ExprPtr r) : lhs(move(l)), rhs(move(r)) {}
UpdateStmt::UpdateStmt(const UpdateStmt &s)
    : lhs(ast::clone(s.lhs)), rhs(ast::clone(s.rhs)) {}
string UpdateStmt::toString() const { return format("(#update {} {})", *lhs, *rhs); }

DelStmt::DelStmt(ExprPtr e) : expr(move(e)) {}
DelStmt::DelStmt(const DelStmt &s) : expr(ast::clone(s.expr)) {}
string DelStmt::toString() const { return format("(#del {})", *expr); }

PrintStmt::PrintStmt(ExprPtr e) : expr(move(e)) {}
PrintStmt::PrintStmt(const PrintStmt &s) : expr(ast::clone(s.expr)) {}
string PrintStmt::toString() const { return format("(#print {})", *expr); }

ReturnStmt::ReturnStmt(ExprPtr e) : expr(move(e)) {}
ReturnStmt::ReturnStmt(const ReturnStmt &s) : expr(ast::clone(s.expr)) {}
string ReturnStmt::toString() const {
  return expr ? format("(#return {})", *expr) : "#return";
}

YieldStmt::YieldStmt(ExprPtr e) : expr(move(e)) {}
YieldStmt::YieldStmt(const YieldStmt &s) : expr(ast::clone(s.expr)) {}
string YieldStmt::toString() const {
  return expr ? format("(#yield {})", *expr) : "#yield";
}

AssertStmt::AssertStmt(ExprPtr e) : expr(move(e)) {}
AssertStmt::AssertStmt(const AssertStmt &s) : expr(ast::clone(s.expr)) {}
string AssertStmt::toString() const { return format("(#assert {})", *expr); }

WhileStmt::WhileStmt(ExprPtr c, StmtPtr s) : cond(move(c)), suite(move(s)) {}
WhileStmt::WhileStmt(const WhileStmt &s)
    : cond(ast::clone(s.cond)), suite(ast::clone(s.suite)) {}
string WhileStmt::toString() const { return format("(#while {} {})", *cond, *suite); }

ForStmt::ForStmt(ExprPtr v, ExprPtr i, StmtPtr s)
    : var(move(v)), iter(move(i)), suite(move(s)) {}
ForStmt::ForStmt(const ForStmt &s)
    : var(ast::clone(s.var)), iter(ast::clone(s.iter)), suite(ast::clone(s.suite)) {}
string ForStmt::toString() const {
  return format("(#for {} {} {})", *var, *iter, *suite);
}

IfStmt::If IfStmt::If::clone() const { return {ast::clone(cond), ast::clone(suite)}; }

IfStmt::IfStmt(vector<IfStmt::If> &&i) : ifs(move(i)) {}
IfStmt::IfStmt(ExprPtr cond, StmtPtr suite) {
  ifs.push_back(If{move(cond), move(suite)});
}
IfStmt::IfStmt(const IfStmt &s) : ifs(ast::clone_nop(s.ifs)) {}
string IfStmt::toString() const {
  string s;
  for (auto &i : ifs)
    s += format(" ({}{})", i.cond ? format(":cond {} ", *i.cond) : "", *i.suite);
  return format("(#if{})", s);
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
    s += format(" ({} {})", *patterns[i], *cases[i]);
  return format("(#match{})", s);
}

ExtendStmt::ExtendStmt(ExprPtr t, StmtPtr s) : type(move(t)), suite(move(s)) {}
ExtendStmt::ExtendStmt(const ExtendStmt &s)
    : type(ast::clone(s.type)), suite(ast::clone(s.suite)), generics(s.generics) {}
string ExtendStmt::toString() const { return format("(#extend {} {})", *type, *suite); }

ImportStmt::ImportStmt(const Item &f, const vector<Item> &w) : from(f), what(w) {}
ImportStmt::ImportStmt(const ImportStmt &s) : from(s.from), what(s.what) {}
string ImportStmt::toString() const {
  vector<string> s;
  for (auto &w : what) {
    s.push_back(w.second.size() ? format("({} :as {})", w.first, w.second) : w.first);
  }
  return format("(#import {}{})",
                from.second.size() ? format("({} :as {})", from.first, from.second)
                                   : from.first,
                s.size() ? format(" :what {}", fmt::join(s, " ")) : "");
}

ExternImportStmt::ExternImportStmt(const ImportStmt::Item &n, ExprPtr f, ExprPtr t,
                                   vector<Param> &&a, const string &l)
    : name(n), from(move(f)), ret(move(t)), args(move(a)), lang(l) {}
ExternImportStmt::ExternImportStmt(const ExternImportStmt &s)
    : name(s.name), from(ast::clone(s.from)), ret(ast::clone(s.ret)),
      args(ast::clone_nop(s.args)), lang(s.lang) {}
string ExternImportStmt::toString() const {
  string as;
  for (auto &a : args)
    as += " " + a.toString();
  return format("(#extern {} :lang {} :typ {}{}{})",
                name.second.size() ? format("({} :as {})", name.first, name.second)
                                   : name.first,
                lang, *ret, args.size() ? " :args" + as : "",
                from ? " :from" + from->toString() : "");
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
    s += format(" ({}{}{})", i.var != "" ? format(":var {} ", i.var) : "",
                i.exc ? format(":exc {} ", *i.exc) : "", *i.suite);
  auto f = format("{}", *finally);
  return format("(#try {}{}{})", *suite, s, f.size() ? format(" :finally {}", f) : "");
}

GlobalStmt::GlobalStmt(const string &v) : var(v) {}
GlobalStmt::GlobalStmt(const GlobalStmt &s) : var(s.var) {}
string GlobalStmt::toString() const { return format("(#global {})", var); }

ThrowStmt::ThrowStmt(ExprPtr e) : expr(move(e)) {}
ThrowStmt::ThrowStmt(const ThrowStmt &s) : expr(ast::clone(s.expr)) {}
string ThrowStmt::toString() const { return format("(#throw {})", *expr); }

FunctionStmt::FunctionStmt(const string &n, ExprPtr r, vector<Param> &&g,
                           vector<Param> &&a, StmtPtr s, const vector<string> &at,
                           const string &c)
    : name(n), ret(move(r)), generics(move(g)), args(move(a)), suite(move(s)),
      attributes(at), className(c) {}
FunctionStmt::FunctionStmt(const FunctionStmt &s)
    : name(s.name), ret(ast::clone(s.ret)), generics(ast::clone_nop(s.generics)),
      args(ast::clone_nop(s.args)), suite(ast::clone(s.suite)),
      attributes(s.attributes), className(s.className) {}
string FunctionStmt::toString() const {
  string gs;
  for (auto &a : generics)
    gs += " " + a.toString();
  string as;
  for (auto &a : args)
    as += " " + a.toString();
  return format(
      "(#fun {}{}{}{}{} {})", name, ret ? " :ret " + ret->toString() : "",
      generics.size() ? format(" :gen{}", gs) : "", args.size() ? " :args" + as : "",
      attributes.size() ? format(" :attrs ({})", fmt::join(attributes, " ")) : "",
      suite ? suite->toString() : "(#pass)");
}

PyDefStmt::PyDefStmt(const string &n, ExprPtr r, vector<Param> &&a, const string &s)
    : name(n), ret(move(r)), args(move(a)), code(s) {}
PyDefStmt::PyDefStmt(const PyDefStmt &s)
    : name(s.name), ret(ast::clone(s.ret)), args(ast::clone_nop(s.args)), code(s.code) {
}
string PyDefStmt::toString() const {
  string as;
  for (auto &a : args)
    as += " " + a.toString();
  return format("(#pydef {}{}{} '{}')", name, ret ? " :ret " + ret->toString() : "",
                args.size() ? " :args" + as : "", escape(code));
}

ClassStmt::ClassStmt(bool i, const string &n, vector<Param> &&g, vector<Param> &&a,
                     StmtPtr s, const vector<string> &at)
    : isRecord(i), name(n), generics(move(g)), args(move(a)), suite(move(s)),
      attributes(at) {}
ClassStmt::ClassStmt(const ClassStmt &s)
    : isRecord(s.isRecord), name(s.name), generics(ast::clone_nop(s.generics)),
      args(ast::clone_nop(s.args)), suite(ast::clone(s.suite)) {}
string ClassStmt::toString() const {
  string gs;
  for (auto &a : generics)
    gs += " " + a.toString();
  string as;
  for (auto &a : args)
    as += " " + a.toString();
  return format(
      "(#{} {}{}{} {} {})", (isRecord ? "type" : "class"), name,
      generics.size() ? format(" :gen{}", gs) : "", args.size() ? " :args" + as : "",
      attributes.size() ? format(" :attrs ({})", fmt::join(attributes, " ")) : "",
      *suite);
}

AssignEqStmt::AssignEqStmt(ExprPtr l, ExprPtr r, const string &o)
    : lhs(move(l)), rhs(move(r)), op(o) {}
AssignEqStmt::AssignEqStmt(const AssignEqStmt &s)
    : lhs(ast::clone(s.lhs)), rhs(ast::clone(s.rhs)), op(s.op) {}
string AssignEqStmt::toString() const {
  return format("(#assigneq {} '{}' {})", *lhs, op, *rhs);
}

YieldFromStmt::YieldFromStmt(ExprPtr e) : expr(move(e)) {}
YieldFromStmt::YieldFromStmt(const YieldFromStmt &s) : expr(ast::clone(s.expr)) {}
string YieldFromStmt::toString() const { return format("(#yieldfrom {})", *expr); }

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
    as.push_back(vars[i].size() ? format("({} :var {})", *items[i], vars[i])
                                : items[i]->toString());
  }
  return format("(#with ({}) {})", fmt::join(as, " "), *suite);
}

Pattern::Pattern() : _type(nullptr) {}
Pattern::Pattern(const Pattern &e) : seq::SrcObject(e), _type(e._type) {}

StarPattern::StarPattern() : Pattern() {}
StarPattern::StarPattern(const StarPattern &p) : Pattern(p) {}
string StarPattern::toString() const { return "#star"; }

IntPattern::IntPattern(int v) : Pattern(), value(v) {}
IntPattern::IntPattern(const IntPattern &p) : Pattern(p), value(p.value) {}
string IntPattern::toString() const { return format("(#int {})", value); }

BoolPattern::BoolPattern(bool v) : Pattern(), value(v) {}
BoolPattern::BoolPattern(const BoolPattern &p) : Pattern(p), value(p.value) {}
string BoolPattern::toString() const { return format("(#bool {})", value); }

StrPattern::StrPattern(const string &v) : Pattern(), value(v) {}
StrPattern::StrPattern(const StrPattern &p) : Pattern(p), value(p.value) {}
string StrPattern::toString() const { return format("(#str '{}')", escape(value)); }

SeqPattern::SeqPattern(const string &v) : Pattern(), value(v) {}
SeqPattern::SeqPattern(const SeqPattern &p) : Pattern(p), value(p.value) {}
string SeqPattern::toString() const { return format("(#seq '{}')", escape(value)); }

RangePattern::RangePattern(int s, int e) : Pattern(), start(s), end(e) {}
RangePattern::RangePattern(const RangePattern &p)
    : Pattern(p), start(p.start), end(p.end) {}
string RangePattern::toString() const { return format("(#range {} {})", start, end); }

TuplePattern::TuplePattern(vector<PatternPtr> &&p) : Pattern(), patterns(move(p)) {}
TuplePattern::TuplePattern(const TuplePattern &p)
    : Pattern(p), patterns(ast::clone(p.patterns)) {}
string TuplePattern::toString() const {
  return format("(#tuple {})", combine(patterns));
}

ListPattern::ListPattern(vector<PatternPtr> &&p) : Pattern(), patterns(move(p)) {}
ListPattern::ListPattern(const ListPattern &p)
    : Pattern(p), patterns(ast::clone(p.patterns)) {}
string ListPattern::toString() const { return format("(#list {})", combine(patterns)); }

OrPattern::OrPattern(vector<PatternPtr> &&p) : Pattern(), patterns(move(p)) {}
OrPattern::OrPattern(const OrPattern &p)
    : Pattern(p), patterns(ast::clone(p.patterns)) {}
string OrPattern::toString() const { return format("(#or {})", combine(patterns)); }

WildcardPattern::WildcardPattern(const string &v) : Pattern(), var(v) {}
WildcardPattern::WildcardPattern(const WildcardPattern &p) : Pattern(p), var(p.var) {}
string WildcardPattern::toString() const {
  return var == "" ? "#wild" : format("(#wild {})", var);
}

GuardedPattern::GuardedPattern(PatternPtr p, ExprPtr c)
    : Pattern(), pattern(move(p)), cond(move(c)) {}
GuardedPattern::GuardedPattern(const GuardedPattern &p)
    : Pattern(p), pattern(ast::clone(p.pattern)), cond(ast::clone(p.cond)) {}
string GuardedPattern::toString() const {
  return format("(#guard {} {})", *pattern, *cond);
}

BoundPattern::BoundPattern(const string &v, PatternPtr p)
    : Pattern(), var(v), pattern(move(p)) {}
BoundPattern::BoundPattern(const BoundPattern &p)
    : Pattern(p), var(p.var), pattern(ast::clone(p.pattern)) {}
string BoundPattern::toString() const {
  return format("(#bound {} {})", var, *pattern);
}

} // namespace ast
} // namespace seq
