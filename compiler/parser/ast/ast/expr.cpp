#include <memory>
#include <string>
#include <vector>

#include "PARSER/ast/ast/expr.h"

using fmt::format;
using std::move;

namespace seq {
namespace ast {

Expr::Expr() : type(nullptr), isTypeExpr(false) {}
Expr::Expr(const Expr &e) : seq::SrcObject(e), type(e.type), isTypeExpr(e.isTypeExpr) {}
Expr::~Expr() {}
types::TypePtr Expr::getType() const { return type; }
void Expr::setType(types::TypePtr t) { type = t; }
bool Expr::isType() const { return isTypeExpr; }
void Expr::markType() { isTypeExpr = true; }
string Expr::wrap(const string &s) const {
  return format("({}{})", s, type ? format(" TYPE={}", type->toString()) : "");
}

Param::Param(string name, ExprPtr type, ExprPtr deflt)
    : name(name), type(move(type)), deflt(move(deflt)) {}
Param Param::clone() const { return {name, ast::clone(type), ast::clone(deflt)}; }
string Param::toString() const {
  return format("({}{}{})", name, type ? " TYPE=" + type->toString() : "",
                deflt ? " DEFAULT=" + deflt->toString() : "");
}

NoneExpr::NoneExpr() : Expr() {}
NoneExpr::NoneExpr(const NoneExpr &e) : Expr(e) {}
string NoneExpr::toString() const { return wrap("NONE"); }

BoolExpr::BoolExpr(bool v) : Expr(), value(v) {}
BoolExpr::BoolExpr(const BoolExpr &e) : Expr(e), value(e.value) {}
string BoolExpr::toString() const { return wrap(format("BOOL {}", int(value))); }

IntExpr::IntExpr(int v)
    : Expr(), value(std::to_string(v)), suffix(""), intValue(v), sign(false) {}
IntExpr::IntExpr(string v, string s)
    : Expr(), value(v), suffix(s), intValue(0), sign(false) {}
IntExpr::IntExpr(const IntExpr &e)
    : Expr(e), value(e.value), suffix(e.suffix), intValue(e.intValue), sign(e.sign) {}
string IntExpr::toString() const {
  return wrap(
      format("INT {}{}", value, suffix == "" ? "" : format(" SUFFIX={}", suffix)));
}

FloatExpr::FloatExpr(double v, string s) : Expr(), value(v), suffix(s) {}
FloatExpr::FloatExpr(const FloatExpr &e) : Expr(e), value(e.value), suffix(e.suffix) {}
string FloatExpr::toString() const {
  return wrap(
      format("FLOAT {}{}", value, suffix == "" ? "" : format(" SUFFIX={}", suffix)));
}

StringExpr::StringExpr(string v, string prefix) : Expr(), value(v), prefix(prefix) {}
StringExpr::StringExpr(const StringExpr &e) : Expr(e), value(e.value) {}
string StringExpr::toString() const {
  return wrap(format("STR '{}'{}", escape(value),
                     prefix == "" ? "" : format(" PREFIX={}", prefix)));
}

IdExpr::IdExpr(const string &v) : Expr(), value(v) {}
IdExpr::IdExpr(const IdExpr &e) : Expr(e), value(e.value) {}
string IdExpr::toString() const { return wrap(format("ID {}", value)); }

StarExpr::StarExpr(ExprPtr v) : Expr(), what(move(v)) {}
StarExpr::StarExpr(const StarExpr &e) : Expr(e), what(ast::clone(e.what)) {}
string StarExpr::toString() const { return wrap(format("STAR {}", *what)); }

TupleExpr::TupleExpr(vector<ExprPtr> &&i) : Expr(), items(move(i)) {}
TupleExpr::TupleExpr(const TupleExpr &e) : Expr(e), items(ast::clone(e.items)) {}
string TupleExpr::toString() const { return wrap(format("TUPLE {}", combine(items))); }

ListExpr::ListExpr(vector<ExprPtr> &&i) : Expr(), items(move(i)) {}
ListExpr::ListExpr(const ListExpr &e) : Expr(e), items(ast::clone(e.items)) {}
string ListExpr::toString() const {
  return wrap(items.size() ? format("LIST {}", combine(items)) : "LIST");
}

SetExpr::SetExpr(vector<ExprPtr> &&i) : Expr(), items(move(i)) {}
SetExpr::SetExpr(const SetExpr &e) : Expr(e), items(ast::clone(e.items)) {}
string SetExpr::toString() const {
  return wrap(items.size() ? format("SET {}", combine(items)) : "SET");
}

DictExpr::DictItem DictExpr::DictItem::clone() const {
  return {ast::clone(key), ast::clone(value)};
}

DictExpr::DictExpr(vector<DictExpr::KeyValue> &&i) : Expr(), items(move(i)) {}
DictExpr::DictExpr(const DictExpr &e) : Expr(e), items(ast::clone_nop(e.items)) {}
string DictExpr::toString() const {
  vector<string> s;
  for (auto &i : items)
    s.push_back(format("{} {}", *i.key, *i.value));
  return wrap(s.size() ? format("DICT {}", fmt::join(s, " ")) : "DICT");
}

GeneratorBody GeneratorBody::clone() const {
  return {ast::clone(vars), ast::clone(gen), ast::clone(conds)};
}

GeneratorExpr::GeneratorExpr(GeneratorExpr::Kind k, ExprPtr e,
                             vector<GeneratorBody> &&l)
    : Expr(), kind(k), expr(move(e)), loops(move(l)) {}
GeneratorExpr::GeneratorExpr(const GeneratorExpr &e)
    : Expr(e), kind(e.kind), expr(ast::clone(e.expr)), loops(ast::clone_nop(e.loops)) {}
string GeneratorExpr::toString() const {
  string prefix = "";
  if (kind == Kind::ListGenerator)
    prefix = "LIST_";
  if (kind == Kind::SetGenerator)
    prefix = "SET_";
  string s;
  for (auto &i : loops) {
    string q;
    for (auto &k : i.conds)
      q += format(" IF {}", *k);
    s += format("FOR {} {}{}", i.vars->toString(), i.gen->toString(), q);
  }
  return wrap(format("{}GENERATOR {}{}", prefix, *expr, s));
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
      q += format(" IF {}", *k);
    s += format("FOR ({}) {}{}", i.vars->toString(), i.gen->toString(), q);
  }
  return wrap(format("DICT_GEN {} {}{}", *key, *expr, s));
}

IfExpr::IfExpr(ExprPtr c, ExprPtr i, ExprPtr e)
    : Expr(), cond(move(c)), eif(move(i)), eelse(move(e)) {}
IfExpr::IfExpr(const IfExpr &e)
    : Expr(e), cond(ast::clone(e.cond)), eif(ast::clone(e.eif)),
      eelse(ast::clone(e.eelse)) {}
string IfExpr::toString() const {
  return wrap(format("IF {} {} {}", *cond, *eif, *eelse));
}

UnaryExpr::UnaryExpr(const string &o, ExprPtr e) : Expr(), op(o), expr(move(e)) {}
UnaryExpr::UnaryExpr(const UnaryExpr &e)
    : Expr(e), op(e.op), expr(ast::clone(e.expr)) {}
string UnaryExpr::toString() const { return wrap(format("UNARY '{}' {}", op, *expr)); }

BinaryExpr::BinaryExpr(ExprPtr l, const string &o, ExprPtr r, bool i)
    : Expr(), op(o), lexpr(move(l)), rexpr(move(r)), inPlace(i) {}
BinaryExpr::BinaryExpr(const BinaryExpr &e)
    : Expr(e), op(e.op), lexpr(ast::clone(e.lexpr)), rexpr(ast::clone(e.rexpr)),
      inPlace(e.inPlace) {}
string BinaryExpr::toString() const {
  return wrap(
      format("BINARY {} '{}' {}{}", *lexpr, op, *rexpr, inPlace ? " INPLACE" : ""));
}

PipeExpr::Pipe PipeExpr::Pipe::clone() const { return {op, ast::clone(expr)}; }

PipeExpr::PipeExpr(vector<PipeExpr::Pipe> &&i) : Expr(), items(move(i)) {}
PipeExpr::PipeExpr(const PipeExpr &e)
    : Expr(e), items(ast::clone_nop(e.items)), inTypes(e.inTypes) {}
string PipeExpr::toString() const {
  vector<string> s;
  for (auto &i : items)
    s.push_back(format("({}{})", *i.expr, i.op == "||>" ? " PARALLEL" : ""));
  return wrap(format("PIPE {}", fmt::join(s, " ")));
}

IndexExpr::IndexExpr(ExprPtr e, ExprPtr i) : Expr(), expr(move(e)), index(move(i)) {}
IndexExpr::IndexExpr(const IndexExpr &e)
    : Expr(e), expr(ast::clone(e.expr)), index(ast::clone(e.index)) {}
string IndexExpr::toString() const {
  return wrap(format("INDEX {} {}", *expr, *index));
}

TupleIndexExpr::TupleIndexExpr(ExprPtr e, int i) : Expr(), expr(move(e)), index(i) {}
TupleIndexExpr::TupleIndexExpr(const TupleIndexExpr &e)
    : Expr(e), expr(ast::clone(e.expr)), index(e.index) {}
string TupleIndexExpr::toString() const {
  return wrap(format("TUPLEINDEX {} {}", *expr, index));
}

StackAllocExpr::StackAllocExpr(ExprPtr t, ExprPtr e)
    : Expr(), typeExpr(move(t)), expr(move(e)) {}
StackAllocExpr::StackAllocExpr(const StackAllocExpr &e)
    : Expr(e), typeExpr(ast::clone(e.typeExpr)), expr(ast::clone(e.expr)) {}
string StackAllocExpr::toString() const {
  return wrap(format("STACKALLOC {} {}", *typeExpr, *expr));
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
      s += format(" ({} NAME={})", *i.value, i.name);
    }
  return wrap(format("CALL {}{}", *expr, s));
}

DotExpr::DotExpr(ExprPtr e, const string &m)
    : Expr(), expr(move(e)), member(m), isMethod(false) {}
DotExpr::DotExpr(const DotExpr &e)
    : Expr(e), expr(ast::clone(e.expr)), member(e.member), isMethod(e.isMethod) {}
string DotExpr::toString() const { return wrap(format("DOT {} {}", *expr, member)); }

SliceExpr::SliceExpr(ExprPtr s, ExprPtr e, ExprPtr st)
    : Expr(), st(move(s)), ed(move(e)), step(move(st)) {}
SliceExpr::SliceExpr(const SliceExpr &e)
    : Expr(e), st(ast::clone(e.st)), ed(ast::clone(e.ed)), step(ast::clone(e.step)) {}
string SliceExpr::toString() const {
  return wrap(format("SLICE{}{}{}", st ? format(" START={}", *st) : "",
                     ed ? format(" END={}", *ed) : "",
                     step ? format(" STEP={}", *step) : ""));
}

EllipsisExpr::EllipsisExpr(bool i) : Expr(), isPipeArg(i) {}
EllipsisExpr::EllipsisExpr(const EllipsisExpr &e) : Expr(e), isPipeArg(e.isPipeArg) {}
string EllipsisExpr::toString() const { return wrap("ELLIPSIS"); }

TypeOfExpr::TypeOfExpr(ExprPtr e) : Expr(), expr(move(e)) {}
TypeOfExpr::TypeOfExpr(const TypeOfExpr &e) : Expr(e), expr(ast::clone(e.expr)) {}
string TypeOfExpr::toString() const { return wrap(format("TYPEOF {}", *expr)); }

PtrExpr::PtrExpr(ExprPtr e) : Expr(), expr(move(e)) {}
PtrExpr::PtrExpr(const PtrExpr &e) : Expr(e), expr(ast::clone(e.expr)) {}
string PtrExpr::toString() const { return wrap(format("PTR {}", *expr)); }

LambdaExpr::LambdaExpr(vector<string> v, ExprPtr e) : Expr(), vars(v), expr(move(e)) {}
LambdaExpr::LambdaExpr(const LambdaExpr &e)
    : Expr(e), vars(e.vars), expr(ast::clone(e.expr)) {}
string LambdaExpr::toString() const {
  return wrap(format("LAMBDA {} {}", fmt::join(vars, " "), *expr));
}

YieldExpr::YieldExpr() : Expr() {}
YieldExpr::YieldExpr(const YieldExpr &e) : Expr(e) {}
string YieldExpr::toString() const { return "YIELD"; }

InstantiateExpr::InstantiateExpr(ExprPtr e, vector<ExprPtr> &&i)
    : Expr(), type(move(e)), params(move(i)) {}
InstantiateExpr::InstantiateExpr(ExprPtr e, ExprPtr t) : Expr(), type(move(e)) {
  params.push_back(move(t));
}
InstantiateExpr::InstantiateExpr(const InstantiateExpr &e)
    : Expr(e), type(ast::clone(e.type)), params(ast::clone(e.params)) {}
string InstantiateExpr::toString() const {
  return wrap(format("INSTANTIATE {} {}", *type, combine(params)));
}

StaticExpr::StaticExpr(ExprPtr e, const std::set<std::string> &c)
    : Expr(), expr(move(e)), captures(c) {}
StaticExpr::StaticExpr(const StaticExpr &e)
    : Expr(e), expr(ast::clone(e.expr)), captures(e.captures) {}
string StaticExpr::toString() const { return wrap(format("STATIC {}", *expr)); }

StmtExpr::StmtExpr(vector<StmtPtr> &&s, ExprPtr e)
    : Expr(), stmts(move(s)), expr(move(e)) {}
StmtExpr::StmtExpr(const StmtExpr &e)
    : Expr(e), stmts(ast::clone(e.stmts)), expr(ast::clone(e.expr)) {}
string StmtExpr::toString() const {
  return wrap(format("STMT ({}) {}", combine(stmts, " "), *expr));
}

} // namespace ast
} // namespace seq
