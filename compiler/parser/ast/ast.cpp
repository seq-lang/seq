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

template <typename T> T CL(const T &v) { return v.clone(); }
template <typename T> unique_ptr<T> CL(const unique_ptr<T> &v) {
  return v ? v->clone() : nullptr;
}
template <typename T> vector<T> CL(const vector<T> &v) {
  vector<T> r;
  for (auto &i : v)
    r.push_back(CL(i));
  return r;
}
template <typename T>
std::string combine(const std::vector<T> &items, std::string delim = " ") {
  std::string s = "";
  for (int i = 0; i < items.size(); i++)
    s += (i ? delim : "") + items[i]->toString();
  return s;
}

Expr::Expr() : _type(nullptr), _isType(false) {}
Expr::Expr(const Expr &e)
    : seq::SrcObject(e), _type(e._type), _isType(e._isType) {}

NoneExpr::NoneExpr() : Expr() {}
NoneExpr::NoneExpr(const NoneExpr &e) : Expr(e) {}
string NoneExpr::toString() const { return "#none"; }

BoolExpr::BoolExpr(bool v) : Expr(), value(v) {}
BoolExpr::BoolExpr(const BoolExpr &e) : Expr(e), value(e.value) {}
string BoolExpr::toString() const { return format("(#bool {})", int(value)); }

IntExpr::IntExpr(int v) : Expr(), value(std::to_string(v)), suffix("") {}
IntExpr::IntExpr(const IntExpr &e)
    : Expr(e), value(e.value), suffix(e.suffix) {}
IntExpr::IntExpr(const string &v, const string &s)
    : Expr(), value(v), suffix(s) {}
string IntExpr::toString() const {
  return format("(#int {}{})", value,
                suffix == "" ? "" : format(" :suffix {}", suffix));
}

FloatExpr::FloatExpr(double v, const string &s) : Expr(), value(v), suffix(s) {}
FloatExpr::FloatExpr(const FloatExpr &e)
    : Expr(e), value(e.value), suffix(e.suffix) {}
string FloatExpr::toString() const {
  return format("(#float {}{})", value,
                suffix == "" ? "" : format(" :suffix {}", suffix));
}

StringExpr::StringExpr(const string &v) : Expr(), value(v) {}
StringExpr::StringExpr(const StringExpr &e) : Expr(e), value(e.value) {}
string StringExpr::toString() const {
  return format("(#str '{}')", escape(value));
}

FStringExpr::FStringExpr(const string &v) : Expr(), value(v) {}
FStringExpr::FStringExpr(const FStringExpr &e) : Expr(e), value(e.value) {}
string FStringExpr::toString() const {
  return format("(#fstr '{}')", escape(value));
}

KmerExpr::KmerExpr(const string &v) : Expr(), value(v) {}
KmerExpr::KmerExpr(const KmerExpr &e) : Expr(e), value(e.value) {}
string KmerExpr::toString() const {
  return format("(#kmer '{}')", escape(value));
}

SeqExpr::SeqExpr(const string &v, const string &p)
    : Expr(), prefix(p), value(v) {}
SeqExpr::SeqExpr(const SeqExpr &e)
    : Expr(e), prefix(e.prefix), value(e.value) {}
string SeqExpr::toString() const {
  return format("(#seq '{}'{})", value,
                prefix == "" ? "" : format(" :prefix {}", prefix));
}

IdExpr::IdExpr(const string &v) : Expr(), value(v) {}
IdExpr::IdExpr(const IdExpr &e) : Expr(e), value(e.value) {}
string IdExpr::toString() const { return format("(#id {})", value); }

UnpackExpr::UnpackExpr(ExprPtr v) : Expr(), what(move(v)) {}
UnpackExpr::UnpackExpr(const UnpackExpr &e) : Expr(e), what(CL(e.what)) {}
string UnpackExpr::toString() const { return format("(#unpack {})", *what); }

TupleExpr::TupleExpr(vector<ExprPtr> &&i) : Expr(), items(move(i)) {}
TupleExpr::TupleExpr(const TupleExpr &e) : Expr(e), items(CL(e.items)) {}
string TupleExpr::toString() const {
  return format("(#tuple {})", combine(items));
}

ListExpr::ListExpr(vector<ExprPtr> &&i) : Expr(), items(move(i)) {}
ListExpr::ListExpr(const ListExpr &e) : Expr(e), items(CL(e.items)) {}
string ListExpr::toString() const {
  return items.size() ? format("(#list {})", combine(items)) : "#list";
}

SetExpr::SetExpr(vector<ExprPtr> &&i) : Expr(), items(move(i)) {}
SetExpr::SetExpr(const SetExpr &e) : Expr(e), items(CL(e.items)) {}
string SetExpr::toString() const {
  return items.size() ? format("(#set {})", combine(items)) : "#set";
}

DictExpr::KeyValue DictExpr::KeyValue::clone() const {
  return {key->clone(), value->clone()};
}

DictExpr::DictExpr(vector<DictExpr::KeyValue> &&i) : Expr(), items(move(i)) {}
DictExpr::DictExpr(const DictExpr &e) : Expr(e), items(CL(e.items)) {}
string DictExpr::toString() const {
  vector<string> s;
  for (auto &i : items)
    s.push_back(format("({} {})", *i.key, *i.value));
  return s.size() ? format("(#dict {})", fmt::join(s, " ")) : "#dict";
}

GeneratorExpr::Body GeneratorExpr::Body::clone() const {
  return {vars, CL(gen), CL(conds)};
}

GeneratorExpr::GeneratorExpr(GeneratorExpr::Kind k, ExprPtr e,
                             vector<GeneratorExpr::Body> &&l)
    : Expr(), kind(k), expr(move(e)), loops(move(l)) {}
GeneratorExpr::GeneratorExpr(const GeneratorExpr &e)
    : Expr(e), kind(e.kind), expr(CL(e.expr)), loops(CL(e.loops)) {}
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
      q += format(" (#if {})", *k);
    s += format("(#for ({}) {}{})", fmt::join(i.vars, " "), i.gen->toString(),
                q);
  }
  return format("(#{}gen {}{})", prefix, *expr, s);
}

DictGeneratorExpr::DictGeneratorExpr(ExprPtr k, ExprPtr e,
                                     vector<GeneratorExpr::Body> &&l)
    : Expr(), key(move(k)), expr(move(e)), loops(move(l)) {}
DictGeneratorExpr::DictGeneratorExpr(const DictGeneratorExpr &e)
    : Expr(e), key(CL(e.key)), expr(CL(e.expr)), loops(CL(e.loops)) {}
string DictGeneratorExpr::toString() const {
  string s;
  for (auto &i : loops) {
    string q;
    for (auto &k : i.conds)
      q += format(" (#if {})", *k);
    s += format("(#for ({}) {}{})", fmt::join(i.vars, " "), i.gen->toString(),
                q);
  }
  return format("(#dict_gen {} {}{})", *key, *expr, s);
}

IfExpr::IfExpr(ExprPtr c, ExprPtr i, ExprPtr e)
    : Expr(), cond(move(c)), eif(move(i)), eelse(move(e)) {}
IfExpr::IfExpr(const IfExpr &e)
    : Expr(e), cond(CL(e.cond)), eif(CL(e.eif)), eelse(CL(e.eelse)) {}
string IfExpr::toString() const {
  return format("(#if {} {} {})", *cond, *eif, *eelse);
}

UnaryExpr::UnaryExpr(const string &o, ExprPtr e)
    : Expr(), op(o), expr(move(e)) {}
UnaryExpr::UnaryExpr(const UnaryExpr &e)
    : Expr(e), op(e.op), expr(CL(e.expr)) {}
string UnaryExpr::toString() const {
  return format("(#unary {} :op '{}')", *expr, op);
}

BinaryExpr::BinaryExpr(ExprPtr l, const string &o, ExprPtr r, bool i)
    : Expr(), op(o), lexpr(move(l)), rexpr(move(r)), inPlace(i) {}
BinaryExpr::BinaryExpr(const BinaryExpr &e)
    : Expr(e), op(e.op), lexpr(CL(e.lexpr)), rexpr(CL(e.rexpr)),
      inPlace(e.inPlace) {}
string BinaryExpr::toString() const {
  return format("(#binary {} {} :op '{}' :inplace {})", *lexpr, *rexpr, op,
                inPlace);
}

PipeExpr::Pipe PipeExpr::Pipe::clone() const { return {op, CL(expr)}; }

PipeExpr::PipeExpr(vector<PipeExpr::Pipe> &&i) : Expr(), items(move(i)) {}
PipeExpr::PipeExpr(const PipeExpr &e) : Expr(e), items(CL(e.items)) {}
string PipeExpr::toString() const {
  vector<string> s;
  for (auto &i : items)
    s.push_back(format("({}{})", *i.expr,
                       i.op.size() ? format(" :op '{}'", i.op) : ""));
  return format("(#pipe {})", fmt::join(s, " "));
}

IndexExpr::IndexExpr(ExprPtr e, ExprPtr i)
    : Expr(), expr(move(e)), index(move(i)) {}
IndexExpr::IndexExpr(const IndexExpr &e)
    : Expr(e), expr(CL(e.expr)), index(CL(e.index)) {}
string IndexExpr::toString() const {
  return format("(#index {} {})", *expr, *index);
}

CallExpr::Arg CallExpr::Arg::clone() const { return {name, CL(value)}; }

CallExpr::CallExpr(ExprPtr e) : Expr(), expr(move(e)) {}
CallExpr::CallExpr(const CallExpr &e)
    : Expr(e), expr(CL(e.expr)), args(CL(e.args)) {}
CallExpr::CallExpr(ExprPtr e, vector<CallExpr::Arg> &&a)
    : Expr(), expr(move(e)), args(move(a)) {}
CallExpr::CallExpr(ExprPtr e, vector<ExprPtr> &&arg) : Expr(), expr(move(e)) {
  for (auto &i : arg) {
    args.push_back(CallExpr::Arg{"", move(i)});
  }
}
CallExpr::CallExpr(ExprPtr e, ExprPtr arg, ExprPtr arg2, ExprPtr arg3)
    : Expr(), expr(move(e)) {
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
  return format("(#call {}{})", *expr, s);
}

DotExpr::DotExpr(ExprPtr e, const string &m)
    : Expr(), expr(move(e)), member(m) {}
DotExpr::DotExpr(const DotExpr &e)
    : Expr(e), expr(CL(e.expr)), member(e.member) {}
string DotExpr::toString() const {
  return format("(#dot {} {})", *expr, member);
}

SliceExpr::SliceExpr(ExprPtr s, ExprPtr e, ExprPtr st)
    : Expr(), st(move(s)), ed(move(e)), step(move(st)) {}
SliceExpr::SliceExpr(const SliceExpr &e)
    : Expr(e), st(CL(e.st)), ed(CL(e.ed)), step(CL(e.step)) {}
string SliceExpr::toString() const {
  return format("(#slice{}{}{})", st ? format(" :start {}", *st) : "",
                ed ? format(" :end {}", *ed) : "",
                step ? format(" :step {}", *step) : "");
}

EllipsisExpr::EllipsisExpr() : Expr() {}
EllipsisExpr::EllipsisExpr(const EllipsisExpr &e) : Expr(e) {}
string EllipsisExpr::toString() const { return "#ellipsis"; }

TypeOfExpr::TypeOfExpr(ExprPtr e) : Expr(), expr(move(e)) {}
TypeOfExpr::TypeOfExpr(const TypeOfExpr &e) : Expr(e), expr(CL(e.expr)) {}
string TypeOfExpr::toString() const { return format("(#typeof {})", *expr); }

PtrExpr::PtrExpr(ExprPtr e) : Expr(), expr(move(e)) {}
PtrExpr::PtrExpr(const PtrExpr &e) : Expr(e), expr(CL(e.expr)) {}
string PtrExpr::toString() const { return format("(#ptr {})", *expr); }

LambdaExpr::LambdaExpr(vector<string> v, ExprPtr e)
    : Expr(), vars(v), expr(move(e)) {}
LambdaExpr::LambdaExpr(const LambdaExpr &e)
    : Expr(e), vars(e.vars), expr(CL(e.expr)) {}
string LambdaExpr::toString() const {
  return format("(#lambda ({}) {})", fmt::join(vars, " "), *expr);
}

YieldExpr::YieldExpr() : Expr() {}
YieldExpr::YieldExpr(const YieldExpr &e) : Expr(e) {}
string YieldExpr::toString() const { return "#yield"; }

Param Param::clone() const { return {name, CL(type), CL(deflt)}; }
string Param::toString() const {
  return format("({}{}{})", name, type ? " :typ " + type->toString() : "",
                deflt ? " :default " + deflt->toString() : "");
}

Stmt::Stmt(const seq::SrcInfo &s) { setSrcInfo(s); }

vector<Stmt *> Stmt::getStatements() { return {this}; }

SuiteStmt::SuiteStmt(vector<StmtPtr> &&s) : stmts(move(s)) {}
SuiteStmt::SuiteStmt(StmtPtr s, StmtPtr s2, StmtPtr s3) {
  stmts.push_back(move(s));
  if (s2)
    stmts.push_back(move(s2));
  if (s3)
    stmts.push_back(move(s3));
}
SuiteStmt::SuiteStmt(const SuiteStmt &s) : stmts(CL(s.stmts)) {}
string SuiteStmt::toString() const {
  return format("({})", combine(stmts, "\n  "));
}
vector<Stmt *> SuiteStmt::getStatements() {
  vector<Stmt *> result;
  for (auto &s : stmts) {
    result.push_back(s.get());
  }
  return result;
}

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
ExprStmt::ExprStmt(const ExprStmt &s) : expr(CL(s.expr)) {}
string ExprStmt::toString() const { return format("(#expr {})", *expr); }

AssignStmt::AssignStmt(ExprPtr l, ExprPtr r, ExprPtr t, bool m, bool f)
    : lhs(move(l)), rhs(move(r)), type(move(t)), mustExist(m), force(f) {}
AssignStmt::AssignStmt(const AssignStmt &s)
    : lhs(CL(s.lhs)), rhs(CL(s.rhs)), type(CL(s.type)), mustExist(s.mustExist),
      force(s.force) {}
string AssignStmt::toString() const {
  return format("(#assign {} {}{})", *lhs, *rhs,
                type ? format(" :type {}", *type) : "");
}

DelStmt::DelStmt(ExprPtr e) : expr(move(e)) {}
DelStmt::DelStmt(const DelStmt &s) : expr(CL(s.expr)) {}
string DelStmt::toString() const { return format("(#del {})", *expr); }

PrintStmt::PrintStmt(ExprPtr e) : expr(move(e)) {}
PrintStmt::PrintStmt(const PrintStmt &s) : expr(CL(s.expr)) {}
string PrintStmt::toString() const { return format("(#print {})", *expr); }

ReturnStmt::ReturnStmt(ExprPtr e) : expr(move(e)) {}
ReturnStmt::ReturnStmt(const ReturnStmt &s) : expr(CL(s.expr)) {}
string ReturnStmt::toString() const {
  return expr ? format("(#return {})", *expr) : "#return";
}

YieldStmt::YieldStmt(ExprPtr e) : expr(move(e)) {}
YieldStmt::YieldStmt(const YieldStmt &s) : expr(CL(s.expr)) {}
string YieldStmt::toString() const {
  return expr ? format("(#yield {})", *expr) : "#yield";
}

AssertStmt::AssertStmt(ExprPtr e) : expr(move(e)) {}
AssertStmt::AssertStmt(const AssertStmt &s) : expr(CL(s.expr)) {}
string AssertStmt::toString() const { return format("(#assert {})", *expr); }

WhileStmt::WhileStmt(ExprPtr c, StmtPtr s) : cond(move(c)), suite(move(s)) {}
WhileStmt::WhileStmt(const WhileStmt &s)
    : cond(CL(s.cond)), suite(CL(s.suite)) {}
string WhileStmt::toString() const {
  return format("(#while {} {})", *cond, *suite);
}

ForStmt::ForStmt(ExprPtr v, ExprPtr i, StmtPtr s)
    : var(move(v)), iter(move(i)), suite(move(s)) {}
ForStmt::ForStmt(const ForStmt &s)
    : var(CL(s.var)), iter(CL(s.iter)), suite(CL(s.suite)) {}
string ForStmt::toString() const {
  return format("(#for {} {} {})", *var, *iter, *suite);
}

IfStmt::If IfStmt::If::clone() const { return {CL(cond), CL(suite)}; }

IfStmt::IfStmt(vector<IfStmt::If> &&i) : ifs(move(i)) {}
IfStmt::IfStmt(ExprPtr cond, StmtPtr suite) {
  ifs.push_back(If{move(cond), move(suite)});
}
IfStmt::IfStmt(const IfStmt &s) : ifs(CL(s.ifs)) {}
string IfStmt::toString() const {
  string s;
  for (auto &i : ifs)
    s +=
        format(" ({}{})", i.cond ? format(":cond {} ", *i.cond) : "", *i.suite);
  return format("(#if{})", s);
}

MatchStmt::MatchStmt(ExprPtr w, vector<PatternPtr> &&p, vector<StmtPtr> &&c)
    : what(move(w)), patterns(move(p)), cases(move(c)) {
  assert(p.size() == c.size());
}
MatchStmt::MatchStmt(ExprPtr w, vector<pair<PatternPtr, StmtPtr>> &&v)
    : what(move(w)) {
  for (auto &i : v) {
    patterns.push_back(move(i.first));
    cases.push_back(move(i.second));
  }
}

MatchStmt::MatchStmt(const MatchStmt &s)
    : what(CL(s.what)), patterns(CL(s.patterns)), cases(CL(s.cases)) {}
string MatchStmt::toString() const {
  string s;
  for (int i = 0; i < patterns.size(); i++)
    s += format(" ({} {})", *patterns[i], *cases[i]);
  return format("(#match{})", s);
}

ExtendStmt::ExtendStmt(ExprPtr e, StmtPtr s) : what(move(e)), suite(move(s)) {}
ExtendStmt::ExtendStmt(const ExtendStmt &s)
    : what(CL(s.what)), suite(CL(s.suite)) {}
string ExtendStmt::toString() const {
  return format("(#extend {} {})", *what, *suite);
}

ImportStmt::ImportStmt(const Item &f, const vector<Item> &w)
    : from(f), what(w) {}
ImportStmt::ImportStmt(const ImportStmt &s) : from(s.from), what(s.what) {}
string ImportStmt::toString() const {
  vector<string> s;
  for (auto &w : what) {
    s.push_back(w.second.size() ? format("({} :as {})", w.first, w.second)
                                : w.first);
  }
  return format("(#import {}{})",
                from.second.size()
                    ? format("({} :as {})", from.first, from.second)
                    : from.first,
                s.size() ? format(" :what {}", fmt::join(s, " ")) : "");
}

ExternImportStmt::ExternImportStmt(const ImportStmt::Item &n, ExprPtr f,
                                   ExprPtr t, vector<Param> &&a,
                                   const string &l)
    : name(n), from(move(f)), ret(move(t)), args(move(a)), lang(l) {}
ExternImportStmt::ExternImportStmt(const ExternImportStmt &s)
    : name(s.name), from(CL(s.from)), ret(CL(s.ret)), args(CL(s.args)),
      lang(s.lang) {}
string ExternImportStmt::toString() const {
  string as;
  for (auto &a : args)
    as += " " + a.toString();
  return format("(#extern {} :lang {} :typ {}{}{})",
                name.second.size()
                    ? format("({} :as {})", name.first, name.second)
                    : name.first,
                lang, *ret, args.size() ? " :args" + as : "",
                from ? " :from" + from->toString() : "");
}

TryStmt::Catch TryStmt::Catch::clone() const {
  return {var, CL(exc), CL(suite)};
}

TryStmt::TryStmt(StmtPtr s, vector<Catch> &&c, StmtPtr f)
    : suite(move(s)), catches(move(c)), finally(move(f)) {}
TryStmt::TryStmt(const TryStmt &s)
    : suite(CL(s.suite)), catches(CL(s.catches)), finally(CL(s.finally)) {}
string TryStmt::toString() const {
  string s;
  for (auto &i : catches)
    s += format(" ({}{}{})", i.var != "" ? format(":var {} ", i.var) : "",
                i.exc ? format(":exc {} ", *i.exc) : "", *i.suite);
  auto f = format("{}", *finally);
  return format("(#try {}{}{})", *suite, s,
                f.size() ? format(" :finally {}", f) : "");
}

GlobalStmt::GlobalStmt(const string &v) : var(v) {}
GlobalStmt::GlobalStmt(const GlobalStmt &s) : var(s.var) {}
string GlobalStmt::toString() const { return format("(#global {})", var); }

ThrowStmt::ThrowStmt(ExprPtr e) : expr(move(e)) {}
ThrowStmt::ThrowStmt(const ThrowStmt &s) : expr(CL(s.expr)) {}
string ThrowStmt::toString() const { return format("(#throw {})", *expr); }

FunctionStmt::FunctionStmt(const string &n, ExprPtr r, const vector<string> &g,
                           vector<Param> &&a, std::shared_ptr<Stmt> s,
                           const vector<string> &at)
    : name(n), ret(move(r)), generics(g), args(move(a)), suite(s),
      attributes(at) {}
FunctionStmt::FunctionStmt(const FunctionStmt &s)
    : name(s.name), ret(CL(s.ret)), generics(s.generics), args(CL(s.args)),
      suite(s.suite), attributes(s.attributes) {}
string FunctionStmt::toString() const {
  string as;
  for (auto &a : args)
    as += " " + a.toString();
  return format(
      "(#fun {}{}{}{}{} {})", name, ret ? " :ret " + ret->toString() : "",
      generics.size() ? format(" :gen {}", fmt::join(generics, " ")) : "",
      args.size() ? " :args" + as : "",
      attributes.size() ? format(" :attrs ({})", fmt::join(attributes, " "))
                        : "",
      suite ? suite->toString() : "(#pass)");
}

PyDefStmt::PyDefStmt(const string &n, ExprPtr r, vector<Param> &&a,
                     const string &s)
    : name(n), ret(move(r)), args(move(a)), code(s) {}
PyDefStmt::PyDefStmt(const PyDefStmt &s)
    : name(s.name), ret(CL(s.ret)), args(CL(s.args)), code(s.code) {}
string PyDefStmt::toString() const {
  string as;
  for (auto &a : args)
    as += " " + a.toString();
  return format("(#pydef {}{}{} '{}')", name,
                ret ? " :ret " + ret->toString() : "",
                args.size() ? " :args" + as : "", escape(code));
}

ClassStmt::ClassStmt(bool i, const string &n, const vector<string> &g,
                     vector<Param> &&a, StmtPtr s)
    : isRecord(i), name(n), generics(g), args(move(a)), suite(move(s)) {}
ClassStmt::ClassStmt(const ClassStmt &s)
    : isRecord(s.isRecord), name(s.name), generics(s.generics),
      args(CL(s.args)), suite(CL(s.suite)) {}
string ClassStmt::toString() const {
  string as;
  for (auto &a : args)
    as += " " + a.toString();
  return format("(#{} {}{}{} {})", (isRecord ? "type" : "class"), name,
                generics.size() ? format(" :gen {}", fmt::join(generics, " "))
                                : "",
                args.size() ? " :args" + as : "", *suite);
}

DeclareStmt::DeclareStmt(Param p) : param(move(p)) {}
DeclareStmt::DeclareStmt(const DeclareStmt &s) : param(CL(s.param)) {}
string DeclareStmt::toString() const {
  return format("(#declare {})", param.toString());
}

AssignEqStmt::AssignEqStmt(ExprPtr l, ExprPtr r, const string &o)
    : lhs(move(l)), rhs(move(r)), op(o) {}
AssignEqStmt::AssignEqStmt(const AssignEqStmt &s)
    : lhs(CL(s.lhs)), rhs(CL(s.rhs)), op(s.op) {}
string AssignEqStmt::toString() const {
  return format("(#assigneq {} {} :op '{}')", *lhs, *rhs, op);
}

YieldFromStmt::YieldFromStmt(ExprPtr e) : expr(move(e)) {}
YieldFromStmt::YieldFromStmt(const YieldFromStmt &s) : expr(CL(s.expr)) {}
string YieldFromStmt::toString() const {
  return format("(#yieldfrom {})", *expr);
}

WithStmt::WithStmt(vector<ExprPtr> &&i, const vector<string> &v, StmtPtr s)
    : items(move(i)), vars(v), suite(move(s)) {
  assert(i.size() == v.size());
}
WithStmt::WithStmt(vector<pair<ExprPtr, string>> &&v, StmtPtr s)
    : suite(move(s)) {
  for (auto &i : v) {
    items.push_back(move(i.first));
    vars.push_back(i.second);
  }
}
WithStmt::WithStmt(const WithStmt &s)
    : items(CL(s.items)), vars(s.vars), suite(CL(s.suite)) {}
string WithStmt::toString() const {
  vector<string> as;
  for (int i = 0; i < items.size(); i++) {
    as.push_back(vars[i].size() ? format("({} :var {})", *items[i], vars[i])
                                : items[i]->toString());
  }
  return format("(#with ({}) {})", fmt::join(as, " "), *suite);
}

StarPattern::StarPattern() {}
StarPattern::StarPattern(const StarPattern &p) {}
string StarPattern::toString() const { return "#star"; }

IntPattern::IntPattern(int v) : value(v) {}
IntPattern::IntPattern(const IntPattern &p) : value(p.value) {}
string IntPattern::toString() const { return format("(#int {})", value); }

BoolPattern::BoolPattern(bool v) : value(v) {}
BoolPattern::BoolPattern(const BoolPattern &p) : value(p.value) {}
string BoolPattern::toString() const { return format("(#bool {})", value); }

StrPattern::StrPattern(const string &v) : value(v) {}
StrPattern::StrPattern(const StrPattern &p) : value(p.value) {}
string StrPattern::toString() const {
  return format("(#str '{}')", escape(value));
}

SeqPattern::SeqPattern(const string &v) : value(v) {}
SeqPattern::SeqPattern(const SeqPattern &p) : value(p.value) {}
string SeqPattern::toString() const {
  return format("(#seq '{}')", escape(value));
}

RangePattern::RangePattern(int s, int e) : start(s), end(e) {}
RangePattern::RangePattern(const RangePattern &p)
    : start(p.start), end(p.end) {}
string RangePattern::toString() const {
  return format("(#range {} {})", start, end);
}

TuplePattern::TuplePattern(vector<PatternPtr> &&p) : patterns(move(p)) {}
TuplePattern::TuplePattern(const TuplePattern &p) : patterns(CL(p.patterns)) {}
string TuplePattern::toString() const {
  return format("(#tuple {})", combine(patterns));
}

ListPattern::ListPattern(vector<PatternPtr> &&p) : patterns(move(p)) {}
ListPattern::ListPattern(const ListPattern &p) : patterns(CL(p.patterns)) {}
string ListPattern::toString() const {
  return format("(#list {})", combine(patterns));
}

OrPattern::OrPattern(vector<PatternPtr> &&p) : patterns(move(p)) {}
OrPattern::OrPattern(const OrPattern &p) : patterns(CL(p.patterns)) {}
string OrPattern::toString() const {
  return format("(#or {})", combine(patterns));
}

WildcardPattern::WildcardPattern(const string &v) : var(v) {}
WildcardPattern::WildcardPattern(const WildcardPattern &p) : var(p.var) {}
string WildcardPattern::toString() const {
  return var == "" ? "#wild" : format("(#wild {})", var);
}

GuardedPattern::GuardedPattern(PatternPtr p, ExprPtr c)
    : pattern(move(p)), cond(move(c)) {}
GuardedPattern::GuardedPattern(const GuardedPattern &p)
    : pattern(CL(p.pattern)), cond(CL(p.cond)) {}
string GuardedPattern::toString() const {
  return format("(#guard {} {})", *pattern, *cond);
}

BoundPattern::BoundPattern(const string &v, PatternPtr p)
    : var(v), pattern(move(p)) {}
BoundPattern::BoundPattern(const BoundPattern &p)
    : var(p.var), pattern(CL(p.pattern)) {}
string BoundPattern::toString() const {
  return format("(#bound {} {})", var, *pattern);
}

} // namespace ast
} // namespace seq
