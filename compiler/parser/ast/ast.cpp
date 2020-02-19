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
string EmptyExpr::to_string() const { return "#empty"; }

BoolExpr::BoolExpr(bool v) : value(v) {}
string BoolExpr::to_string() const { return format("(#bool {})", int(value)); }

IntExpr::IntExpr(int v) : value(std::to_string(v)), suffix("") {}
IntExpr::IntExpr(string v, string s) : value(v), suffix(s) {}
string IntExpr::to_string() const {
  return format("(#int {}{})", value,
                suffix == "" ? "" : format(" :suffix {}", suffix));
}

FloatExpr::FloatExpr(double v, string s) : value(v), suffix(s) {}
string FloatExpr::to_string() const {
  return format("(#float {}{})", value,
                suffix == "" ? "" : format(" :suffix {}", suffix));
}

StringExpr::StringExpr(string v) : value(v) {}
string StringExpr::to_string() const {
  return format("(#str '{}')", escape(value));
}

FStringExpr::FStringExpr(string v) : value(v) {}
string FStringExpr::to_string() const {
  return format("(#fstr '{}')", escape(value));
}

KmerExpr::KmerExpr(string v) : value(v) {}
string KmerExpr::to_string() const {
  return format("(#kmer '{}')", escape(value));
}

SeqExpr::SeqExpr(string v, string p) : prefix(p), value(v) {}
string SeqExpr::to_string() const {
  return format("(#seq '{}'{})", value,
                prefix == "" ? "" : format(" :prefix {}", prefix));
}

IdExpr::IdExpr(string v) : value(v) {}
string IdExpr::to_string() const { return format("(#id {})", value); }

UnpackExpr::UnpackExpr(ExprPtr v) : what(move(v)) {}
string UnpackExpr::to_string() const { return format("(#unpack {})", *what); }

TupleExpr::TupleExpr(vector<ExprPtr> i) : items(move(i)) {}
string TupleExpr::to_string() const {
  return format("(#tuple {})", combine(items));
}

ListExpr::ListExpr(vector<ExprPtr> i) : items(move(i)) {}
string ListExpr::to_string() const {
  return items.size() ? format("(#list {})", combine(items)) : "#list";
}

SetExpr::SetExpr(vector<ExprPtr> i) : items(move(i)) {}
string SetExpr::to_string() const {
  return items.size() ? format("(#set {})", combine(items)) : "#set";
}

DictExpr::DictExpr(vector<DictExpr::KeyValue> i) : items(move(i)) {}
string DictExpr::to_string() const {
  vector<string> s;
  for (auto &i : items)
    s.push_back(format("({} {})", *i.key, *i.value));
  return s.size() ? format("(#dict {})", fmt::join(s, " ")) : "#dict";
}

GeneratorExpr::GeneratorExpr(GeneratorExpr::Kind k, ExprPtr e,
                             vector<GeneratorExpr::Body> l)
    : kind(k), expr(move(e)), loops(move(l)) {}
string GeneratorExpr::to_string() const {
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
    s += format("(#for ({}) {}{})", fmt::join(i.vars, " "), i.gen->to_string(),
                q);
  }
  return format("(#{}gen {}{})", prefix, *expr, s);
}

DictGeneratorExpr::DictGeneratorExpr(ExprPtr k, ExprPtr e,
                                     vector<GeneratorExpr::Body> l)
    : key(move(k)), expr(move(e)), loops(move(l)) {}
string DictGeneratorExpr::to_string() const {
  string s;
  for (auto &i : loops) {
    string q;
    for (auto &k : i.conds)
      q += format(" (#if {})", *k);
    s += format("(#for ({}) {}{})", fmt::join(i.vars, " "), i.gen->to_string(),
                q);
  }
  return format("(#dict_gen {} {}{})", *key, *expr, s);
}

IfExpr::IfExpr(ExprPtr c, ExprPtr i, ExprPtr e)
    : cond(move(c)), eif(move(i)), eelse(move(e)) {}
string IfExpr::to_string() const {
  return format("(#if {} {} {})", *cond, *eif, *eelse);
}

UnaryExpr::UnaryExpr(string o, ExprPtr e) : op(o), expr(move(e)) {}
string UnaryExpr::to_string() const {
  return format("(#unary {} :op '{}')", *expr, op);
}

BinaryExpr::BinaryExpr(ExprPtr l, string o, ExprPtr r, bool i)
    : op(o), lexpr(move(l)), rexpr(move(r)), inPlace(i) {}
string BinaryExpr::to_string() const {
  return format("(#binary {} {} :op '{}' :inplace {})", *lexpr, *rexpr, op,
                inPlace);
}

PipeExpr::PipeExpr(vector<PipeExpr::Pipe> i) : items(move(i)) {}
string PipeExpr::to_string() const {
  vector<string> s;
  for (auto &i : items)
    s.push_back(format("({}{})", *i.expr,
                       i.op.size() ? format(" :op '{}'", i.op) : ""));
  return format("(#pipe {})", fmt::join(s, " "));
}

IndexExpr::IndexExpr(ExprPtr e, ExprPtr i) : expr(move(e)), index(move(i)) {}
string IndexExpr::to_string() const {
  return format("(#index {} {})", *expr, *index);
}

CallExpr::CallExpr(ExprPtr e) : expr(move(e)) {}
CallExpr::CallExpr(ExprPtr e, vector<CallExpr::Arg> a)
    : expr(move(e)), args(move(a)) {}
CallExpr::CallExpr(ExprPtr e, vector<ExprPtr> arg) : expr(move(e)) {
  for (auto &i : arg) {
    args.push_back(CallExpr::Arg{"", move(i)});
  }
}
CallExpr::CallExpr(ExprPtr e, ExprPtr arg) : expr(move(e)) {
  args.push_back(CallExpr::Arg{"", move(arg)});
}
string CallExpr::to_string() const {
  string s;
  for (auto &i : args)
    if (i.name == "") {
      s += " " + i.value->to_string();
    } else {
      s += format(" ({} :name {})", *i.value, i.name);
    }
  return format("(#call {}{})", *expr, s);
}

DotExpr::DotExpr(ExprPtr e, string m) : expr(move(e)), member(m) {}
string DotExpr::to_string() const {
  return format("(#dot {} {})", *expr, member);
}

SliceExpr::SliceExpr(ExprPtr s, ExprPtr e, ExprPtr st)
    : st(move(s)), ed(move(e)), step(move(st)) {}
string SliceExpr::to_string() const {
  return format("(#slice{}{}{})", st ? format(" :start {}", *st) : "",
                ed ? format(" :end {}", *ed) : "",
                step ? format(" :step {}", *step) : "");
}

EllipsisExpr::EllipsisExpr() {}
string EllipsisExpr::to_string() const { return "#ellipsis"; }

TypeOfExpr::TypeOfExpr(ExprPtr e) : expr(move(e)) {}
string TypeOfExpr::to_string() const { return format("(#typeof {})", *expr); }

PtrExpr::PtrExpr(ExprPtr e) : expr(move(e)) {}
string PtrExpr::to_string() const { return format("(#ptr {})", *expr); }

LambdaExpr::LambdaExpr(vector<string> v, ExprPtr e) : vars(v), expr(move(e)) {}
string LambdaExpr::to_string() const {
  return format("(#lambda ({}) {})", fmt::join(vars, " "), *expr);
}
YieldExpr::YieldExpr() {}
string YieldExpr::to_string() const { return "#yield"; }

string Param::to_string() const {
  return format("({}{}{})", name, type ? " :typ " + type->to_string() : "",
                deflt ? " :default " + deflt->to_string() : "");
}

Stmt::Stmt(const seq::SrcInfo &s) { setSrcInfo(s); }

vector<Stmt *> Stmt::getStatements() { return {this}; }

SuiteStmt::SuiteStmt(vector<StmtPtr> s) : stmts(move(s)) {}
string SuiteStmt::to_string() const {
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
string PassStmt::to_string() const { return "#pass"; }

BreakStmt::BreakStmt() {}
string BreakStmt::to_string() const { return "#break"; }

ContinueStmt::ContinueStmt() {}
string ContinueStmt::to_string() const { return "#continue"; }

ExprStmt::ExprStmt(ExprPtr e) : expr(move(e)) {}
string ExprStmt::to_string() const { return format("(#expr {})", *expr); }

AssignStmt::AssignStmt(ExprPtr l, ExprPtr r, ExprPtr t, bool m)
    : lhs(move(l)), rhs(move(r)), type(move(t)), mustExist(m) {}
string AssignStmt::to_string() const {
  return format("(#assign {} {}{})", *lhs, *rhs,
                type ? format(" :type {}", *type) : "");
}

DelStmt::DelStmt(ExprPtr e) : expr(move(e)) {}
string DelStmt::to_string() const { return format("(#del {})", *expr); }

PrintStmt::PrintStmt(ExprPtr e) : expr(move(e)) {}
string PrintStmt::to_string() const { return format("(#print {})", *expr); }

ReturnStmt::ReturnStmt(ExprPtr e) : expr(move(e)) {}
string ReturnStmt::to_string() const {
  return expr ? format("(#return {})", *expr) : "#return";
}

YieldStmt::YieldStmt(ExprPtr e) : expr(move(e)) {}
string YieldStmt::to_string() const {
  return expr ? format("(#yield {})", *expr) : "#yield";
}

AssertStmt::AssertStmt(ExprPtr e) : expr(move(e)) {}
string AssertStmt::to_string() const { return format("(#assert {})", *expr); }

TypeAliasStmt::TypeAliasStmt(string n, ExprPtr e) : name(n), expr(move(e)) {}
string TypeAliasStmt::to_string() const { return format("(#alias {})", *expr); }

WhileStmt::WhileStmt(ExprPtr c, StmtPtr s) : cond(move(c)), suite(move(s)) {}
string WhileStmt::to_string() const {
  return format("(#while {} {})", *cond, *suite);
}

ForStmt::ForStmt(ExprPtr v, ExprPtr i, StmtPtr s)
    : var(move(v)), iter(move(i)), suite(move(s)) {}
string ForStmt::to_string() const {
  return format("(#for {} {} {})", *var, *iter, *suite);
}

IfStmt::IfStmt(vector<IfStmt::If> i) : ifs(move(i)) {}
string IfStmt::to_string() const {
  string s;
  for (auto &i : ifs)
    s +=
        format(" ({}{})", i.cond ? format(":cond {} ", *i.cond) : "", *i.suite);
  return format("(#if{})", s);
}

MatchStmt::MatchStmt(ExprPtr w, vector<pair<PatternPtr, StmtPtr>> c)
    : what(move(w)), cases(move(c)) {}
string MatchStmt::to_string() const {
  string s;
  for (auto &i : cases)
    s += format(" ({} {})", *i.first, *i.second);
  return format("(#match{})", s);
}

ImportStmt::ImportStmt(Item f, vector<Item> w) : from(f), what(w) {}
string ImportStmt::to_string() const {
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
string ExternImportStmt::to_string() const {
  string as;
  for (auto &a : args)
    as += " " + a.to_string();
  return format("(#extern {} :lang {} :typ {}{}{})",
                name.second.size()
                    ? format("({} :as {})", name.first, name.second)
                    : name.first,
                lang, *ret, args.size() ? " :args" + as : "",
                from ? " :from" + from->to_string() : "");
}

ExtendStmt::ExtendStmt(ExprPtr e, StmtPtr s) : what(move(e)), suite(move(s)) {}
string ExtendStmt::to_string() const {
  return format("(#extend {} {})", *what, *suite);
}

TryStmt::TryStmt(StmtPtr s, vector<Catch> c, StmtPtr f)
    : suite(move(s)), catches(move(c)), finally(move(f)) {}
string TryStmt::to_string() const {
  string s;
  for (auto &i : catches)
    s += format(" ({}{}{})", i.var != "" ? format(":var {} ", i.var) : "",
                i.exc ? format(":exc {} ", *i.exc) : "", *i.suite);
  auto f = format("{}", *finally);
  return format("(#try {}{}{})", *suite, s,
                f.size() ? format(" :finally {}", f) : "");
}

GlobalStmt::GlobalStmt(string v) : var(v) {}
string GlobalStmt::to_string() const { return format("(#global {})", var); }

ThrowStmt::ThrowStmt(ExprPtr e) : expr(move(e)) {}
string ThrowStmt::to_string() const { return format("(#throw {})", *expr); }

FunctionStmt::FunctionStmt(string n, ExprPtr r, vector<string> g,
                           vector<Param> a, StmtPtr s, vector<string> at)
    : name(n), ret(move(r)), generics(g), args(move(a)), suite(move(s)),
      attributes(at) {}
string FunctionStmt::to_string() const {
  string as;
  for (auto &a : args)
    as += " " + a.to_string();
  return format(
      "(#fun {}{}{}{}{} {})", name, ret ? " :ret " + ret->to_string() : "",
      generics.size() ? format(" :gen {}", fmt::join(generics, " ")) : "",
      args.size() ? " :args" + as : "",
      attributes.size() ? format(" :attrs ({})", fmt::join(attributes, " "))
                        : "",
      *suite);
}

PyDefStmt::PyDefStmt(string n, ExprPtr r, vector<Param> a, string s)
    : name(n), ret(move(r)), args(move(a)), code(s) {}
string PyDefStmt::to_string() const {
  string as;
  for (auto &a : args)
    as += " " + a.to_string();
  return format("(#pydef {}{}{} '{}')", name,
                ret ? " :ret " + ret->to_string() : "",
                args.size() ? " :args" + as : "", escape(code));
}

ClassStmt::ClassStmt(bool i, string n, vector<string> g, vector<Param> a,
                     StmtPtr s)
    : isType(i), name(n), generics(g), args(move(a)), suite(move(s)) {}
string ClassStmt::to_string() const {
  string as;
  for (auto &a : args)
    as += " " + a.to_string();
  return format("(#{} {}{}{} {})", (isType ? "type" : "class"), name,
                generics.size() ? format(" :gen {}", fmt::join(generics, " "))
                                : "",
                args.size() ? " :args" + as : "", *suite);
}

DeclareStmt::DeclareStmt(Param p) : param(move(p)) {}
string DeclareStmt::to_string() const {
  return format("(#declare {})", param.to_string());
}

AssignEqStmt::AssignEqStmt(ExprPtr l, ExprPtr r, string o)
    : lhs(move(l)), rhs(move(r)), op(o) {}
string AssignEqStmt::to_string() const {
  return format("(#assigneq {} {} :op '{}')", *lhs, *rhs, op);
}

YieldFromStmt::YieldFromStmt(ExprPtr e) : expr(move(e)) {}
string YieldFromStmt::to_string() const {
  return format("(#yieldfrom {})", *expr);
}

WithStmt::WithStmt(vector<Item> i, StmtPtr s)
    : items(move(i)), suite(move(s)) {}
string WithStmt::to_string() const {
  vector<string> as;
  for (auto &a : items) {
    as.push_back(a.second.size() ? format("({} :var {})", *a.first, a.second)
                                 : a.first->to_string());
  }
  return format("(#with ({}) {})", fmt::join(as, " "), *suite);
}

StarPattern::StarPattern() {}
string StarPattern::to_string() const { return "#star"; }

IntPattern::IntPattern(int v) : value(v) {}
string IntPattern::to_string() const { return format("(#int {})", value); }

BoolPattern::BoolPattern(bool v) : value(v) {}
string BoolPattern::to_string() const { return format("(#bool {})", value); }

StrPattern::StrPattern(string v) : value(v) {}
string StrPattern::to_string() const {
  return format("(#str '{}')", escape(value));
}

SeqPattern::SeqPattern(string v) : value(v) {}
string SeqPattern::to_string() const {
  return format("(#seq '{}')", escape(value));
}

RangePattern::RangePattern(int s, int e) : start(s), end(e) {}
string RangePattern::to_string() const {
  return format("(#range {} {})", start, end);
}

TuplePattern::TuplePattern(vector<PatternPtr> p) : patterns(move(p)) {}
string TuplePattern::to_string() const {
  return format("(#tuple {})", combine(patterns));
}

ListPattern::ListPattern(vector<PatternPtr> p) : patterns(move(p)) {}
string ListPattern::to_string() const {
  return format("(#list {})", combine(patterns));
}

OrPattern::OrPattern(vector<PatternPtr> p) : patterns(move(p)) {}
string OrPattern::to_string() const {
  return format("(#or {})", combine(patterns));
}

WildcardPattern::WildcardPattern(string v) : var(v) {}
string WildcardPattern::to_string() const {
  return var == "" ? "#wild" : format("(#wild {})", var);
}

GuardedPattern::GuardedPattern(PatternPtr p, ExprPtr c)
    : pattern(move(p)), cond(move(c)) {}
string GuardedPattern::to_string() const {
  return format("(#guard {} {})", *pattern, *cond);
}

BoundPattern::BoundPattern(string v, PatternPtr p) : var(v), pattern(move(p)) {}
string BoundPattern::to_string() const {
  return format("(#bound {} {})", var, *pattern);
}

} // namespace ast
} // namespace seq
