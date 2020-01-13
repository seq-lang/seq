#include <memory>
#include <ostream>
#include <string>
#include <tuple>
#include <vector>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include "parser/common.h"
#include "parser/stmt.h"

using fmt::format;
using std::move;
using std::ostream;
using std::string;
using std::tuple;
using std::unique_ptr;
using std::vector;

string Param::to_string() const {
  return format("({}{}{})", name, type ? " :typ " + type->to_string() : "",
                deflt ? " :default " + deflt->to_string() : "");
}

SuiteStmt::SuiteStmt(vector<StmtPtr> s) : stmts(move(s)) {}
string SuiteStmt::to_string() const {
  return format("({})", combine(stmts));
}

PassStmt::PassStmt() {}
string PassStmt::to_string() const { return "#pass"; }

BreakStmt::BreakStmt() {}
string BreakStmt::to_string() const { return "#break"; }

ContinueStmt::ContinueStmt() {}
string ContinueStmt::to_string() const { return "#continue"; }

ExprStmt::ExprStmt(ExprPtr e) : expr(move(e)) {}
string ExprStmt::to_string() const { return format("(#expr {})", *expr); }

AssignStmt::AssignStmt(ExprPtr l, ExprPtr r, int k, ExprPtr t)
    : lhs(move(l)), rhs(move(r)), type(move(t)), kind(k) {}
string AssignStmt::to_string() const {
  return format("(#assign {} {} :kind {}{})", *lhs, *rhs, kind,
                type ? format(" :type {}", *type) : "");
}

DelStmt::DelStmt(ExprPtr e) : expr(move(e)) {}
string DelStmt::to_string() const { return format("(#del {})", *expr); }

PrintStmt::PrintStmt(ExprPtr i): terminator("") {
  items.push_back(move(i));
}
PrintStmt::PrintStmt(vector<ExprPtr> i, string t)
    : items(move(i)), terminator(t) {}
string PrintStmt::to_string() const {
  return format("(#print {} :end '{}')", combine(items), escape(terminator));
}

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

WhileStmt::WhileStmt(ExprPtr c, SuiteStmtPtr s) : cond(move(c)), suite(move(s)) {}
string WhileStmt::to_string() const {
  return format("(#while {} {})", *cond, *suite);
}

ForStmt::ForStmt(vector<string> v, ExprPtr i, SuiteStmtPtr s)
    : vars(v), iter(move(i)), suite(move(s)) {}
string ForStmt::to_string() const {
  return format("(#for ({}) {} {})", fmt::join(vars, " "), *iter, *suite);
}

IfStmt::IfStmt(vector<IfStmt::If> i) : ifs(move(i)) {}
string IfStmt::to_string() const {
  string s;
  for (auto &i : ifs)
    s += format(" ({}{})", i.cond ? format(":cond {} ", *i.cond) : "",
                i.suite);
  return format("(#if{})", s);
}

MatchStmt::MatchStmt(ExprPtr w, vector<pair<PatternPtr, SuiteStmtPtr>> c) : what(move(w)), cases(move(c)) {}
string MatchStmt::to_string() const {
  string s;
  for (auto &i : cases)
    s += format(" ({} {})", *i.first, *i.second);
  return format("(#match{})", s);
}

ImportStmt::ImportStmt(Item f, vector<Item> w)
    : from(f), what(w) {}
string ImportStmt::to_string() const {
  vector<string> s;
  for (auto &w: what) {
    s.push_back(w.second.size() ? format("({} :as {})", w.first, w.second) : w.first);
  }
  return format("(#import {}{})",
    from.second.size() ? format("({} :as {})", from.first, from.second) : from.first,
    s.size() ? format(" :what {}", fmt::join(s, " ")) : ""
  );
}

ExternImportStmt::ExternImportStmt(ImportStmt::Item n, ExprPtr f, ExprPtr t, vector<Param> a, string l)
    : name(n), from(move(f)), ret(move(t)), args(move(a)), lang(l) {}
string ExternImportStmt::to_string() const {
  string as;
  for (auto &a : args)
    as += " " + a.to_string();
  return format("(#extern {} :lang {} :typ {}{}{})",
    name.second.size() ? format("({} :as {})", name.first, name.second) : name.first,
    lang,
    *ret,
    args.size() ? " :args" + as : "",
    from ? " :from" + from->to_string() : ""
  );
}

ExtendStmt::ExtendStmt(ExprPtr e, SuiteStmtPtr s)
    : what(move(e)), suite(move(s)) {}
string ExtendStmt::to_string() const {
  return format("(#extend {} {})", *what, *suite);
}


TryStmt::TryStmt(SuiteStmtPtr s, vector<Catch> c, SuiteStmtPtr f)
    : suite(move(s)), catches(move(c)), finally(move(f)) {}
string TryStmt::to_string() const {
  string s;
  for (auto &i : catches)
    s += format(" ({}{}{})", i.var != "" ? format(":var {} ", i.var) : "",
                i.exc ? format(":exc {} ", *i.exc) : "", *i.suite);
  return format("(#try {}{}{})", *suite, s,
                finally->stmts.size() ? format(" :finally {}", *finally) : "");
}

GlobalStmt::GlobalStmt(string v) : var(v) {}
string GlobalStmt::to_string() const { return format("(#global {})", var); }

ThrowStmt::ThrowStmt(ExprPtr e) : expr(move(e)) {}
string ThrowStmt::to_string() const { return format("(#throw {})", *expr); }

PrefetchStmt::PrefetchStmt(vector<ExprPtr> w) : what(move(w)) {}
string PrefetchStmt::to_string() const {
  return format("(#prefetch {})", combine(what));
}

FunctionStmt::FunctionStmt(string n, ExprPtr r, vector<string> g,
                           vector<Param> a, SuiteStmtPtr s, vector<string> at)
    : name(n), ret(move(r)), generics(g), args(move(a)), suite(move(s)),
      attributes(at) {}
string FunctionStmt::to_string() const {
  string as;
  for (auto &a : args)
    as += " " + a.to_string();
  return format("(#fun {}{}{}{}{} {})", name,
                ret ? " :ret " + ret->to_string() : "",
                generics.size() ? format(" :gen {}", fmt::join(generics, " ")) : "",
                args.size() ? " :args" + as : "",
                attributes.size()
                    ? format(" :attrs ({})", fmt::join(attributes, " "))
                    : "",
                *suite);
}

ClassStmt::ClassStmt(bool i, string n, vector<string> g, vector<Param> a,
                     SuiteStmtPtr s)
    : is_type(i), name(n), generics(g), args(move(a)), suite(move(s)) {}
string ClassStmt::to_string() const {
  string as;
  for (auto &a : args)
    as += " " + a.to_string();
  return format("(#{} {}{}{} {})", (is_type ? "type" : "class"), name,
                generics.size() ? format(" :gen {}", fmt::join(generics, " ")) : "",
                args.size() ? " :args" + as : "", *suite);
}

DeclareStmt::DeclareStmt(Param p) : param(move(p)) {}
string DeclareStmt::to_string() const { return format("(#declare {})", param.to_string()); }