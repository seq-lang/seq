#include "util/fmt/format.h"
#include "util/fmt/ostream.h"
#include <memory>
#include <ostream>
#include <string>
#include <tuple>
#include <vector>

#include "parser/ast/ast.h"
#include "parser/common.h"

using fmt::format;
using std::get;
using std::move;
using std::ostream;
using std::pair;
using std::string;
using std::unique_ptr;
using std::vector;

namespace seq {
namespace ast {

EmptyExpr::EmptyExpr() {}
string EmptyExpr::toString() const { return "#empty"; }

BoolExpr::BoolExpr(bool v) : Expr(), value(v) {}
string BoolExpr::toString() const { return format("(#bool {})", int(value)); }

IntExpr::IntExpr(int v) : Expr(), value(std::to_string(v)), suffix("") {}
IntExpr::IntExpr(string v, string s) : Expr(), value(v), suffix(s) {}
string IntExpr::toString() const {
  return format("(#int {}{})", value,
                suffix == "" ? "" : format(" :suffix {}", suffix));
}

FloatExpr::FloatExpr(double v, string s) : Expr(), value(v), suffix(s) {}
string FloatExpr::toString() const {
  return format("(#float {}{})", value,
                suffix == "" ? "" : format(" :suffix {}", suffix));
}

StringExpr::StringExpr(string v) : Expr(), value(v) {}
string StringExpr::toString() const {
  return format("(#str '{}')", escape(value));
}

FStringExpr::FStringExpr(string v) : Expr(), value(v) {}
string FStringExpr::toString() const {
  return format("(#fstr '{}')", escape(value));
}

KmerExpr::KmerExpr(string v) : Expr(), value(v) {}
string KmerExpr::toString() const {
  return format("(#kmer '{}')", escape(value));
}

SeqExpr::SeqExpr(string v, string p) : Expr(), prefix(p), value(v) {}
string SeqExpr::toString() const {
  return format("(#seq '{}'{})", value,
                prefix == "" ? "" : format(" :prefix {}", prefix));
}

IdExpr::IdExpr(string v) : Expr(), value(v) {}
string IdExpr::toString() const { return format("(#id {})", value); }

UnpackExpr::UnpackExpr(ExprPtr v) : Expr(), what(move(v)) {}
string UnpackExpr::toString() const { return format("(#unpack {})", *what); }

TupleExpr::TupleExpr(vector<ExprPtr> i) : Expr(), items(move(i)) {}
string TupleExpr::toString() const {
  return format("(#tuple {})", combine(items));
}

ListExpr::ListExpr(vector<ExprPtr> i) : Expr(), items(move(i)) {}
string ListExpr::toString() const {
  return items.size() ? format("(#list {})", combine(items)) : "#list";
}

SetExpr::SetExpr(vector<ExprPtr> i) : Expr(), items(move(i)) {}
string SetExpr::toString() const {
  return items.size() ? format("(#set {})", combine(items)) : "#set";
}

DictExpr::DictExpr(vector<DictExpr::KeyValue> i) : Expr(), items(move(i)) {}
string DictExpr::toString() const {
  vector<string> s;
  for (auto &i : items)
    s.push_back(format("({} {})", *i.key, *i.value));
  return s.size() ? format("(#dict {})", fmt::join(s, " ")) : "#dict";
}

GeneratorExpr::GeneratorExpr(GeneratorExpr::Kind k, ExprPtr e,
                             vector<GeneratorExpr::Body> l)
    : kind(k), expr(move(e)), loops(move(l)) {}
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
                                     vector<GeneratorExpr::Body> l)
    : key(move(k)), expr(move(e)), loops(move(l)) {}
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
    : cond(move(c)), eif(move(i)), eelse(move(e)) {}
string IfExpr::toString() const {
  return format("(#if {} {} {})", *cond, *eif, *eelse);
}

UnaryExpr::UnaryExpr(string o, ExprPtr e) : Expr(), op(o), expr(move(e)) {}
string UnaryExpr::toString() const {
  return format("(#unary {} :op '{}')", *expr, op);
}

BinaryExpr::BinaryExpr(ExprPtr l, string o, ExprPtr r, bool i)
    : op(o), lexpr(move(l)), rexpr(move(r)), inPlace(i) {}
string BinaryExpr::toString() const {
  return format("(#binary {} {} :op '{}' :inplace {})", *lexpr, *rexpr, op,
                inPlace);
}

PipeExpr::PipeExpr(vector<PipeExpr::Pipe> i) : Expr(), items(move(i)) {}
string PipeExpr::toString() const {
  vector<string> s;
  for (auto &i : items)
    s.push_back(format("({}{})", *i.expr,
                       i.op.size() ? format(" :op '{}'", i.op) : ""));
  return format("(#pipe {})", fmt::join(s, " "));
}

IndexExpr::IndexExpr(ExprPtr e, ExprPtr i)
    : Expr(), expr(move(e)), index(move(i)) {}
string IndexExpr::toString() const {
  return format("(#index {} {})", *expr, *index);
}

CallExpr::CallExpr(ExprPtr e) : Expr(), expr(move(e)) {}
CallExpr::CallExpr(ExprPtr e, vector<CallExpr::Arg> a)
    : expr(move(e)), args(move(a)) {}
CallExpr::CallExpr(ExprPtr e, vector<ExprPtr> arg) : Expr(), expr(move(e)) {
  for (auto &i : arg) {
    args.push_back(CallExpr::Arg{"", move(i)});
  }
}
CallExpr::CallExpr(ExprPtr e, ExprPtr arg) : Expr(), expr(move(e)) {
  args.push_back(CallExpr::Arg{"", move(arg)});
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

DotExpr::DotExpr(ExprPtr e, string m) : Expr(), expr(move(e)), member(m) {}
string DotExpr::toString() const {
  return format("(#dot {} {})", *expr, member);
}

SliceExpr::SliceExpr(ExprPtr s, ExprPtr e, ExprPtr st)
    : st(move(s)), ed(move(e)), step(move(st)) {}
string SliceExpr::toString() const {
  return format("(#slice{}{}{})", st ? format(" :start {}", *st) : "",
                ed ? format(" :end {}", *ed) : "",
                step ? format(" :step {}", *step) : "");
}

EllipsisExpr::EllipsisExpr() {}
string EllipsisExpr::toString() const { return "#ellipsis"; }

TypeOfExpr::TypeOfExpr(ExprPtr e) : Expr(), expr(move(e)) {}
string TypeOfExpr::toString() const { return format("(#typeof {})", *expr); }

PtrExpr::PtrExpr(ExprPtr e) : Expr(), expr(move(e)) {}
string PtrExpr::toString() const { return format("(#ptr {})", *expr); }

LambdaExpr::LambdaExpr(vector<string> v, ExprPtr e)
    : Expr(), vars(v), expr(move(e)) {}
string LambdaExpr::toString() const {
  return format("(#lambda ({}) {})", fmt::join(vars, " "), *expr);
}
YieldExpr::YieldExpr() {}
string YieldExpr::toString() const { return "#yield"; }

string Param::toString() const {
  return format("({}{}{})", name, type ? " :typ " + type->toString() : "",
                deflt ? " :default " + deflt->toString() : "");
}

Stmt::Stmt(const seq::SrcInfo &s) { setSrcInfo(s); }

vector<Stmt *> Stmt::getStatements() { return {this}; }

SuiteStmt::SuiteStmt(vector<StmtPtr> s) : stmts(move(s)) {}
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
string PassStmt::toString() const { return "#pass"; }

BreakStmt::BreakStmt() {}
string BreakStmt::toString() const { return "#break"; }

ContinueStmt::ContinueStmt() {}
string ContinueStmt::toString() const { return "#continue"; }

ExprStmt::ExprStmt(ExprPtr e) : expr(move(e)) {}
string ExprStmt::toString() const { return format("(#expr {})", *expr); }

AssignStmt::AssignStmt(ExprPtr l, ExprPtr r, ExprPtr t, bool m, bool f)
    : lhs(move(l)), rhs(move(r)), type(move(t)), mustExist(m), force(f) {}
string AssignStmt::toString() const {
  return format("(#assign {} {}{})", *lhs, *rhs,
                type ? format(" :type {}", *type) : "");
}

DelStmt::DelStmt(ExprPtr e) : expr(move(e)) {}
string DelStmt::toString() const { return format("(#del {})", *expr); }

PrintStmt::PrintStmt(ExprPtr e) : expr(move(e)) {}
string PrintStmt::toString() const { return format("(#print {})", *expr); }

ReturnStmt::ReturnStmt(ExprPtr e) : expr(move(e)) {}
string ReturnStmt::toString() const {
  return expr ? format("(#return {})", *expr) : "#return";
}

YieldStmt::YieldStmt(ExprPtr e) : expr(move(e)) {}
string YieldStmt::toString() const {
  return expr ? format("(#yield {})", *expr) : "#yield";
}

AssertStmt::AssertStmt(ExprPtr e) : expr(move(e)) {}
string AssertStmt::toString() const { return format("(#assert {})", *expr); }

TypeAliasStmt::TypeAliasStmt(string n, ExprPtr e) : name(n), expr(move(e)) {}
string TypeAliasStmt::toString() const { return format("(#alias {})", *expr); }

WhileStmt::WhileStmt(ExprPtr c, StmtPtr s) : cond(move(c)), suite(move(s)) {}
string WhileStmt::toString() const {
  return format("(#while {} {})", *cond, *suite);
}

ForStmt::ForStmt(ExprPtr v, ExprPtr i, StmtPtr s)
    : var(move(v)), iter(move(i)), suite(move(s)) {}
string ForStmt::toString() const {
  return format("(#for {} {} {})", *var, *iter, *suite);
}

IfStmt::IfStmt(vector<IfStmt::If> i) : ifs(move(i)) {}
string IfStmt::toString() const {
  string s;
  for (auto &i : ifs)
    s +=
        format(" ({}{})", i.cond ? format(":cond {} ", *i.cond) : "", *i.suite);
  return format("(#if{})", s);
}

MatchStmt::MatchStmt(ExprPtr w, vector<pair<PatternPtr, StmtPtr>> c)
    : what(move(w)), cases(move(c)) {}
string MatchStmt::toString() const {
  string s;
  for (auto &i : cases)
    s += format(" ({} {})", *i.first, *i.second);
  return format("(#match{})", s);
}

ImportStmt::ImportStmt(Item f, vector<Item> w) : from(f), what(w) {}
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

ExternImportStmt::ExternImportStmt(ImportStmt::Item n, ExprPtr f, ExprPtr t,
                                   vector<Param> a, string l)
    : name(n), from(move(f)), ret(move(t)), args(move(a)), lang(l) {}
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

ExtendStmt::ExtendStmt(ExprPtr e, StmtPtr s) : what(move(e)), suite(move(s)) {}
string ExtendStmt::toString() const {
  return format("(#extend {} {})", *what, *suite);
}

TryStmt::TryStmt(StmtPtr s, vector<Catch> c, StmtPtr f)
    : suite(move(s)), catches(move(c)), finally(move(f)) {}
string TryStmt::toString() const {
  string s;
  for (auto &i : catches)
    s += format(" ({}{}{})", i.var != "" ? format(":var {} ", i.var) : "",
                i.exc ? format(":exc {} ", *i.exc) : "", *i.suite);
  auto f = format("{}", *finally);
  return format("(#try {}{}{})", *suite, s,
                f.size() ? format(" :finally {}", f) : "");
}

GlobalStmt::GlobalStmt(string v) : var(v) {}
string GlobalStmt::toString() const { return format("(#global {})", var); }

ThrowStmt::ThrowStmt(ExprPtr e) : expr(move(e)) {}
string ThrowStmt::toString() const { return format("(#throw {})", *expr); }

FunctionStmt::FunctionStmt(string n, ExprPtr r, vector<string> g,
                           vector<Param> a, std::shared_ptr<Stmt> s,
                           vector<string> at)
    : name(n), ret(move(r)), generics(g), args(move(a)), suite(s),
      attributes(at) {}
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
      *suite);
}

PyDefStmt::PyDefStmt(string n, ExprPtr r, vector<Param> a, string s)
    : name(n), ret(move(r)), args(move(a)), code(s) {}
string PyDefStmt::toString() const {
  string as;
  for (auto &a : args)
    as += " " + a.toString();
  return format("(#pydef {}{}{} '{}')", name,
                ret ? " :ret " + ret->toString() : "",
                args.size() ? " :args" + as : "", escape(code));
}

ClassStmt::ClassStmt(bool i, string n, vector<string> g, vector<Param> a,
                     StmtPtr s)
    : isRecord(i), name(n), generics(g), args(move(a)), suite(move(s)) {}
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
string DeclareStmt::toString() const {
  return format("(#declare {})", param.toString());
}

AssignEqStmt::AssignEqStmt(ExprPtr l, ExprPtr r, string o)
    : lhs(move(l)), rhs(move(r)), op(o) {}
string AssignEqStmt::toString() const {
  return format("(#assigneq {} {} :op '{}')", *lhs, *rhs, op);
}

YieldFromStmt::YieldFromStmt(ExprPtr e) : expr(move(e)) {}
string YieldFromStmt::toString() const {
  return format("(#yieldfrom {})", *expr);
}

WithStmt::WithStmt(vector<Item> i, StmtPtr s)
    : items(move(i)), suite(move(s)) {}
string WithStmt::toString() const {
  vector<string> as;
  for (auto &a : items) {
    as.push_back(a.second.size() ? format("({} :var {})", *a.first, a.second)
                                 : a.first->toString());
  }
  return format("(#with ({}) {})", fmt::join(as, " "), *suite);
}

StarPattern::StarPattern() {}
string StarPattern::toString() const { return "#star"; }

IntPattern::IntPattern(int v) : value(v) {}
string IntPattern::toString() const { return format("(#int {})", value); }

BoolPattern::BoolPattern(bool v) : value(v) {}
string BoolPattern::toString() const { return format("(#bool {})", value); }

StrPattern::StrPattern(string v) : value(v) {}
string StrPattern::toString() const {
  return format("(#str '{}')", escape(value));
}

SeqPattern::SeqPattern(string v) : value(v) {}
string SeqPattern::toString() const {
  return format("(#seq '{}')", escape(value));
}

RangePattern::RangePattern(int s, int e) : start(s), end(e) {}
string RangePattern::toString() const {
  return format("(#range {} {})", start, end);
}

TuplePattern::TuplePattern(vector<PatternPtr> p) : patterns(move(p)) {}
string TuplePattern::toString() const {
  return format("(#tuple {})", combine(patterns));
}

ListPattern::ListPattern(vector<PatternPtr> p) : patterns(move(p)) {}
string ListPattern::toString() const {
  return format("(#list {})", combine(patterns));
}

OrPattern::OrPattern(vector<PatternPtr> p) : patterns(move(p)) {}
string OrPattern::toString() const {
  return format("(#or {})", combine(patterns));
}

WildcardPattern::WildcardPattern(string v) : var(v) {}
string WildcardPattern::toString() const {
  return var == "" ? "#wild" : format("(#wild {})", var);
}

GuardedPattern::GuardedPattern(PatternPtr p, ExprPtr c)
    : pattern(move(p)), cond(move(c)) {}
string GuardedPattern::toString() const {
  return format("(#guard {} {})", *pattern, *cond);
}

BoundPattern::BoundPattern(string v, PatternPtr p) : var(v), pattern(move(p)) {}
string BoundPattern::toString() const {
  return format("(#bound {} {})", var, *pattern);
}

} // namespace ast
} // namespace seq
