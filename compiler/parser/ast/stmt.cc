#include <memory>
#include <ostream>
#include <string>
#include <tuple>
#include <vector>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include "parser/ast/stmt.h"
#include "parser/common.h"

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
    s += format(" ({}{})", i.cond ? format(":cond {} ", *i.cond) : "", i.suite);
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

PrefetchStmt::PrefetchStmt(ExprPtr e) : expr(move(e)) {}
string PrefetchStmt::to_string() const {
  return format("(#prefetch {})", *expr);
}

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