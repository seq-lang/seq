#include "util/fmt/format.h"
#include "util/fmt/ostream.h"
#include <memory>
#include <ostream>
#include <string>
#include <tuple>
#include <vector>

#include "parser/ast/expr.h"
#include "parser/common.h"

using fmt::format;
using std::get;
using std::move;
using std::ostream;
using std::string;
using std::unique_ptr;
using std::vector;

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
