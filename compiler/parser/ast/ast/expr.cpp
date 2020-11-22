#include <memory>
#include <string>
#include <vector>

#include "parser/ast/ast/expr.h"
#include "parser/ast/ast/stmt.h"

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
string Expr::wrap(string s) const {
  return format("({}{})", s, type ? format(" TYPE={}", type->toString()) : "");
}

Param::Param(string name, ExprPtr type, ExprPtr deflt)
    : name(name), type(move(type)), deflt(move(deflt)) {}
string Param::toString() const {
  return format("({}{}{})", name, type ? " TYPE=" + type->toString() : "",
                deflt ? " DEFAULT=" + deflt->toString() : "");
}
Param Param::clone() const { return {name, ast::clone(type), ast::clone(deflt)}; }

NoneExpr::NoneExpr() : Expr() {}
NoneExpr::NoneExpr(const NoneExpr &e) : Expr(e) {}
string NoneExpr::toString() const { return wrap("NONE"); }
ExprPtr NoneExpr::clone() const { return make_unique<NoneExpr>(*this); }

BoolExpr::BoolExpr(bool v) : Expr(), value(v) {}
BoolExpr::BoolExpr(const BoolExpr &e) : Expr(e), value(e.value) {}
string BoolExpr::toString() const { return wrap(format("BOOL {}", int(value))); }
ExprPtr BoolExpr::clone() const { return make_unique<BoolExpr>(*this); }

IntExpr::IntExpr(long long v, bool s)
    : Expr(), value(std::to_string(v)), suffix(""), intValue(v), sign(s) {}
IntExpr::IntExpr(string v, string s) : Expr(), suffix(s), intValue(0), sign(false) {
  value = "";
  for (auto c : v)
    if (c != '_')
      value += c;
}
IntExpr::IntExpr(const IntExpr &e)
    : Expr(e), value(e.value), suffix(e.suffix), intValue(e.intValue), sign(e.sign) {}
string IntExpr::toString() const {
  return wrap(
      format("INT {}{}", value, suffix == "" ? "" : format(" SUFFIX={}", suffix)));
}
ExprPtr IntExpr::clone() const { return make_unique<IntExpr>(*this); }

FloatExpr::FloatExpr(double v, string s) : Expr(), value(v), suffix(s) {}
FloatExpr::FloatExpr(const FloatExpr &e) : Expr(e), value(e.value), suffix(e.suffix) {}
string FloatExpr::toString() const {
  return wrap(
      format("FLOAT {}{}", value, suffix == "" ? "" : format(" SUFFIX={}", suffix)));
}
ExprPtr FloatExpr::clone() const { return make_unique<FloatExpr>(*this); }

StringExpr::StringExpr(string v, string prefix) : Expr(), value(v), prefix(prefix) {}
StringExpr::StringExpr(const StringExpr &e)
    : Expr(e), value(e.value), prefix(e.prefix) {}
string StringExpr::toString() const {
  return wrap(format("STR '{}'{}", escape(value),
                     prefix == "" ? "" : format(" PREFIX={}", prefix)));
}
ExprPtr StringExpr::clone() const { return make_unique<StringExpr>(*this); }

IdExpr::IdExpr(string v) : Expr(), value(v) {}
IdExpr::IdExpr(const IdExpr &e) : Expr(e), value(e.value) {}
string IdExpr::toString() const { return wrap(format("ID {}", value)); }
ExprPtr IdExpr::clone() const { return make_unique<IdExpr>(*this); }

StarExpr::StarExpr(ExprPtr v) : Expr(), what(move(v)) {}
StarExpr::StarExpr(const StarExpr &e) : Expr(e), what(ast::clone(e.what)) {}
string StarExpr::toString() const { return wrap(format("STAR {}", *what)); }
ExprPtr StarExpr::clone() const { return make_unique<StarExpr>(*this); }

TupleExpr::TupleExpr(vector<ExprPtr> &&i) : Expr(), items(move(i)) {}
TupleExpr::TupleExpr(const TupleExpr &e) : Expr(e), items(ast::clone(e.items)) {}
string TupleExpr::toString() const { return wrap(format("TUPLE {}", combine(items))); }
ExprPtr TupleExpr::clone() const { return make_unique<TupleExpr>(*this); }

ListExpr::ListExpr(vector<ExprPtr> &&i) : Expr(), items(move(i)) {}
ListExpr::ListExpr(const ListExpr &e) : Expr(e), items(ast::clone(e.items)) {}
string ListExpr::toString() const {
  return wrap(items.size() ? format("LIST {}", combine(items)) : "LIST");
}
ExprPtr ListExpr::clone() const { return make_unique<ListExpr>(*this); }

SetExpr::SetExpr(vector<ExprPtr> &&i) : Expr(), items(move(i)) {}
SetExpr::SetExpr(const SetExpr &e) : Expr(e), items(ast::clone(e.items)) {}
string SetExpr::toString() const {
  return wrap(items.size() ? format("SET {}", combine(items)) : "SET");
}
ExprPtr SetExpr::clone() const { return make_unique<SetExpr>(*this); }

DictExpr::DictItem DictExpr::DictItem::clone() const {
  return {ast::clone(key), ast::clone(value)};
}

DictExpr::DictExpr(vector<DictExpr::DictItem> &&i) : Expr(), items(move(i)) {}
DictExpr::DictExpr(const DictExpr &e) : Expr(e), items(ast::clone_nop(e.items)) {}
string DictExpr::toString() const {
  vector<string> s;
  for (auto &i : items)
    s.push_back(format("{} {}", *i.key, *i.value));
  return wrap(s.size() ? format("DICT {}", fmt::join(s, " ")) : "DICT");
}
ExprPtr DictExpr::clone() const { return make_unique<DictExpr>(*this); }

GeneratorBody GeneratorBody::clone() const {
  return {ast::clone(vars), ast::clone(gen), ast::clone(conds)};
}

GeneratorExpr::GeneratorExpr(GeneratorExpr::GeneratorKind k, ExprPtr e,
                             vector<GeneratorBody> &&l)
    : Expr(), kind(k), expr(move(e)), loops(move(l)) {}
GeneratorExpr::GeneratorExpr(const GeneratorExpr &e)
    : Expr(e), kind(e.kind), expr(ast::clone(e.expr)), loops(ast::clone_nop(e.loops)) {}
string GeneratorExpr::toString() const {
  string prefix = "";
  if (kind == GeneratorKind::ListGenerator)
    prefix = "LIST_";
  if (kind == GeneratorKind::SetGenerator)
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
ExprPtr GeneratorExpr::clone() const { return make_unique<GeneratorExpr>(*this); }

DictGeneratorExpr::DictGeneratorExpr(ExprPtr k, ExprPtr e, vector<GeneratorBody> &&l)
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
ExprPtr DictGeneratorExpr::clone() const {
  return make_unique<DictGeneratorExpr>(*this);
}

IfExpr::IfExpr(ExprPtr c, ExprPtr i, ExprPtr e)
    : Expr(), cond(move(c)), eif(move(i)), eelse(move(e)) {}
IfExpr::IfExpr(const IfExpr &e)
    : Expr(e), cond(ast::clone(e.cond)), eif(ast::clone(e.eif)),
      eelse(ast::clone(e.eelse)) {}
string IfExpr::toString() const {
  return wrap(format("IF {} {} {}", *cond, *eif, *eelse));
}
ExprPtr IfExpr::clone() const { return make_unique<IfExpr>(*this); }

UnaryExpr::UnaryExpr(string o, ExprPtr e) : Expr(), op(o), expr(move(e)) {}
UnaryExpr::UnaryExpr(const UnaryExpr &e)
    : Expr(e), op(e.op), expr(ast::clone(e.expr)) {}
string UnaryExpr::toString() const { return wrap(format("UNARY '{}' {}", op, *expr)); }
ExprPtr UnaryExpr::clone() const { return make_unique<UnaryExpr>(*this); }

BinaryExpr::BinaryExpr(ExprPtr l, string o, ExprPtr r, bool i)
    : Expr(), op(o), lexpr(move(l)), rexpr(move(r)), inPlace(i) {}
BinaryExpr::BinaryExpr(const BinaryExpr &e)
    : Expr(e), op(e.op), lexpr(ast::clone(e.lexpr)), rexpr(ast::clone(e.rexpr)),
      inPlace(e.inPlace) {}
string BinaryExpr::toString() const {
  return wrap(
      format("BINARY {} '{}' {}{}", *lexpr, op, *rexpr, inPlace ? " INPLACE" : ""));
}
ExprPtr BinaryExpr::clone() const { return make_unique<BinaryExpr>(*this); }

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
ExprPtr PipeExpr::clone() const { return make_unique<PipeExpr>(*this); }

IndexExpr::IndexExpr(ExprPtr e, ExprPtr i) : Expr(), expr(move(e)), index(move(i)) {}
IndexExpr::IndexExpr(const IndexExpr &e)
    : Expr(e), expr(ast::clone(e.expr)), index(ast::clone(e.index)) {}
string IndexExpr::toString() const {
  return wrap(format("INDEX {} {}", *expr, *index));
}
ExprPtr IndexExpr::clone() const { return make_unique<IndexExpr>(*this); }

TupleIndexExpr::TupleIndexExpr(ExprPtr e, int i) : Expr(), expr(move(e)), index(i) {}
TupleIndexExpr::TupleIndexExpr(const TupleIndexExpr &e)
    : Expr(e), expr(ast::clone(e.expr)), index(e.index) {}
string TupleIndexExpr::toString() const {
  return wrap(format("TUPLE_INDEX {} {}", *expr, index));
}
ExprPtr TupleIndexExpr::clone() const { return make_unique<TupleIndexExpr>(*this); }

StackAllocExpr::StackAllocExpr(ExprPtr t, ExprPtr e)
    : Expr(), typeExpr(move(t)), expr(move(e)) {}
StackAllocExpr::StackAllocExpr(const StackAllocExpr &e)
    : Expr(e), typeExpr(ast::clone(e.typeExpr)), expr(ast::clone(e.expr)) {}
string StackAllocExpr::toString() const {
  return wrap(format("STACK_ALLOC {} {}", *typeExpr, *expr));
}
ExprPtr StackAllocExpr::clone() const { return make_unique<StackAllocExpr>(*this); }

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
ExprPtr CallExpr::clone() const { return make_unique<CallExpr>(*this); }

DotExpr::DotExpr(ExprPtr e, string m) : Expr(), expr(move(e)), member(m) {}
DotExpr::DotExpr(const DotExpr &e)
    : Expr(e), expr(ast::clone(e.expr)), member(e.member) {}
string DotExpr::toString() const { return wrap(format("DOT {} {}", *expr, member)); }
ExprPtr DotExpr::clone() const { return make_unique<DotExpr>(*this); }

SliceExpr::SliceExpr(ExprPtr s, ExprPtr e, ExprPtr st)
    : Expr(), st(move(s)), ed(move(e)), step(move(st)) {}
SliceExpr::SliceExpr(const SliceExpr &e)
    : Expr(e), st(ast::clone(e.st)), ed(ast::clone(e.ed)), step(ast::clone(e.step)) {}
string SliceExpr::toString() const {
  return wrap(format("SLICE{}{}{}", st ? format(" START={}", *st) : "",
                     ed ? format(" END={}", *ed) : "",
                     step ? format(" STEP={}", *step) : ""));
}
ExprPtr SliceExpr::clone() const { return make_unique<SliceExpr>(*this); }

EllipsisExpr::EllipsisExpr(bool i) : Expr(), isPipeArg(i) {}
EllipsisExpr::EllipsisExpr(const EllipsisExpr &e) : Expr(e), isPipeArg(e.isPipeArg) {}
string EllipsisExpr::toString() const { return wrap("ELLIPSIS"); }
ExprPtr EllipsisExpr::clone() const { return make_unique<EllipsisExpr>(*this); }

TypeOfExpr::TypeOfExpr(ExprPtr e) : Expr(), expr(move(e)) {}
TypeOfExpr::TypeOfExpr(const TypeOfExpr &e) : Expr(e), expr(ast::clone(e.expr)) {}
string TypeOfExpr::toString() const { return wrap(format("TYPEOF {}", *expr)); }
ExprPtr TypeOfExpr::clone() const { return make_unique<TypeOfExpr>(*this); }

PtrExpr::PtrExpr(ExprPtr e) : Expr(), expr(move(e)) {}
PtrExpr::PtrExpr(const PtrExpr &e) : Expr(e), expr(ast::clone(e.expr)) {}
string PtrExpr::toString() const { return wrap(format("PTR {}", *expr)); }
ExprPtr PtrExpr::clone() const { return make_unique<PtrExpr>(*this); }

LambdaExpr::LambdaExpr(vector<string> &&v, ExprPtr e)
    : Expr(), vars(v), expr(move(e)) {}
LambdaExpr::LambdaExpr(const LambdaExpr &e)
    : Expr(e), vars(e.vars), expr(ast::clone(e.expr)) {}
string LambdaExpr::toString() const {
  return wrap(format("LAMBDA {} {}", fmt::join(vars, " "), *expr));
}
ExprPtr LambdaExpr::clone() const { return make_unique<LambdaExpr>(*this); }

YieldExpr::YieldExpr() : Expr() {}
YieldExpr::YieldExpr(const YieldExpr &e) : Expr(e) {}
string YieldExpr::toString() const { return "YIELD"; }
ExprPtr YieldExpr::clone() const { return make_unique<YieldExpr>(*this); }

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
ExprPtr InstantiateExpr::clone() const { return make_unique<InstantiateExpr>(*this); }

StaticExpr::StaticExpr(ExprPtr e, set<string> &&c)
    : Expr(), expr(move(e)), captures(c) {}
StaticExpr::StaticExpr(const StaticExpr &e)
    : Expr(e), expr(ast::clone(e.expr)), captures(e.captures) {}
string StaticExpr::toString() const { return wrap(format("STATIC {}", *expr)); }
ExprPtr StaticExpr::clone() const { return make_unique<StaticExpr>(*this); }

StmtExpr::StmtExpr(vector<StmtPtr> &&s, ExprPtr e)
    : Expr(), stmts(move(s)), expr(move(e)) {}
StmtExpr::StmtExpr(const StmtExpr &e)
    : Expr(e), stmts(ast::clone(e.stmts)), expr(ast::clone(e.expr)) {}
string StmtExpr::toString() const {
  return wrap(format("STMT ({}) {}", combine(stmts, " "), *expr));
}
ExprPtr StmtExpr::clone() const { return make_unique<StmtExpr>(*this); }

} // namespace ast
} // namespace seq
