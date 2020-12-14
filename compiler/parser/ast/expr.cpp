/*
 * expr.cpp --- Seq AST expressions.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <memory>
#include <string>
#include <vector>

#include "expr.h"
#include "parser/ast.h"
#include "parser/visitors/visitor.h"

using fmt::format;
using std::move;

namespace seq {
namespace ast {

Expr::Expr() : type(nullptr), isTypeExpr(false) {}
types::TypePtr Expr::getType() const { return type; }
void Expr::setType(types::TypePtr t) { this->type = move(t); }
bool Expr::isType() const { return isTypeExpr; }
void Expr::markType() { isTypeExpr = true; }
string Expr::wrapType(const string &sexpr) const {
  return format("({}{})", sexpr, type ? format(" TYPE={}", type->toString()) : "");
}

Param::Param(string name, ExprPtr type, ExprPtr deflt)
    : name(move(name)), type(move(type)), deflt(move(deflt)) {}
string Param::toString() const {
  return format("({}{}{})", name, type ? " TYPE=" + type->toString() : "",
                deflt ? " DEFAULT=" + deflt->toString() : "");
}
Param Param::clone() const { return Param(name, ast::clone(type), ast::clone(deflt)); }

NoneExpr::NoneExpr() : Expr() {}
string NoneExpr::toString() const { return wrapType("NONE"); }
ExprPtr NoneExpr::clone() const { return make_unique<NoneExpr>(*this); }
void NoneExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

BoolExpr::BoolExpr(bool value) : Expr(), value(value) {}
string BoolExpr::toString() const { return wrapType(format("BOOL {}", int(value))); }
ExprPtr BoolExpr::clone() const { return make_unique<BoolExpr>(*this); }
void BoolExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

IntExpr::IntExpr(long long intValue, bool sign)
    : Expr(), value(std::to_string(intValue)), intValue(intValue), sign(sign) {}
IntExpr::IntExpr(const string &value, string suffix)
    : Expr(), value(), suffix(move(suffix)), intValue(0), sign(false) {
  for (auto c : value)
    if (c != '_')
      this->value += c;
}
string IntExpr::toString() const {
  return wrapType(
      format("INT {}{}", value, suffix.empty() ? "" : format(" SUFFIX={}", suffix)));
}
ExprPtr IntExpr::clone() const { return make_unique<IntExpr>(*this); }
void IntExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

FloatExpr::FloatExpr(double value, string suffix)
    : Expr(), value(value), suffix(move(suffix)) {}
string FloatExpr::toString() const {
  return wrapType(
      format("FLOAT {}{}", value, suffix.empty() ? "" : format(" SUFFIX={}", suffix)));
}
ExprPtr FloatExpr::clone() const { return make_unique<FloatExpr>(*this); }
void FloatExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

StringExpr::StringExpr(string value, string prefix)
    : Expr(), value(move(value)), prefix(move(prefix)) {}
string StringExpr::toString() const {
  return wrapType(format("STR '{}'{}", escape(value),
                         prefix.empty() ? "" : format(" PREFIX={}", prefix)));
}
ExprPtr StringExpr::clone() const { return make_unique<StringExpr>(*this); }
void StringExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

IdExpr::IdExpr(string value) : Expr(), value(move(value)) {}
string IdExpr::toString() const { return wrapType(format("ID {}", value)); }
ExprPtr IdExpr::clone() const { return make_unique<IdExpr>(*this); }
void IdExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

StarExpr::StarExpr(ExprPtr what) : Expr(), what(move(what)) {}
StarExpr::StarExpr(const StarExpr &expr) : Expr(expr), what(ast::clone(expr.what)) {}
string StarExpr::toString() const {
  return wrapType(format("STAR {}", what->toString()));
}
ExprPtr StarExpr::clone() const { return make_unique<StarExpr>(*this); }
void StarExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

TupleExpr::TupleExpr(vector<ExprPtr> &&items) : Expr(), items(move(items)) {}
TupleExpr::TupleExpr(const TupleExpr &expr)
    : Expr(expr), items(ast::clone(expr.items)) {}
string TupleExpr::toString() const {
  return wrapType(format("TUPLE {}", combine(items)));
}
ExprPtr TupleExpr::clone() const { return make_unique<TupleExpr>(*this); }
void TupleExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

ListExpr::ListExpr(vector<ExprPtr> &&items) : Expr(), items(move(items)) {}
ListExpr::ListExpr(const ListExpr &expr) : Expr(expr), items(ast::clone(expr.items)) {}
string ListExpr::toString() const {
  return wrapType(!items.empty() ? format("LIST {}", combine(items)) : "LIST");
}
ExprPtr ListExpr::clone() const { return make_unique<ListExpr>(*this); }
void ListExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

SetExpr::SetExpr(vector<ExprPtr> &&items) : Expr(), items(move(items)) {}
SetExpr::SetExpr(const SetExpr &expr) : Expr(expr), items(ast::clone(expr.items)) {}
string SetExpr::toString() const {
  return wrapType(!items.empty() ? format("SET {}", combine(items)) : "SET");
}
ExprPtr SetExpr::clone() const { return make_unique<SetExpr>(*this); }
void SetExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

DictExpr::DictItem DictExpr::DictItem::clone() const {
  return {ast::clone(key), ast::clone(value)};
}

DictExpr::DictExpr(vector<DictExpr::DictItem> &&items) : Expr(), items(move(items)) {}
DictExpr::DictExpr(const DictExpr &expr)
    : Expr(expr), items(ast::clone_nop(expr.items)) {}
string DictExpr::toString() const {
  vector<string> s;
  for (auto &i : items)
    s.push_back(format("{} {}", i.key->toString(), i.value->toString()));
  return wrapType(!s.empty() ? format("DICT {}", join(s, " ")) : "DICT");
}
ExprPtr DictExpr::clone() const { return make_unique<DictExpr>(*this); }
void DictExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

GeneratorBody GeneratorBody::clone() const {
  return {ast::clone(vars), ast::clone(gen), ast::clone(conds)};
}

GeneratorExpr::GeneratorExpr(GeneratorExpr::GeneratorKind kind, ExprPtr expr,
                             vector<GeneratorBody> &&loops)
    : Expr(), kind(kind), expr(move(expr)), loops(move(loops)) {}
GeneratorExpr::GeneratorExpr(const GeneratorExpr &expr)
    : Expr(expr), kind(expr.kind), expr(ast::clone(expr.expr)),
      loops(ast::clone_nop(expr.loops)) {}
string GeneratorExpr::toString() const {
  string prefix;
  if (kind == GeneratorKind::ListGenerator)
    prefix = "LIST_";
  if (kind == GeneratorKind::SetGenerator)
    prefix = "SET_";
  string s;
  for (auto &i : loops) {
    string q;
    for (auto &k : i.conds)
      q += format(" IF {}", k->toString());
    s += format("FOR {} {}{}", i.vars->toString(), i.gen->toString(), q);
  }
  return wrapType(format("{}GENERATOR {}{}", prefix, expr->toString(), s));
}
ExprPtr GeneratorExpr::clone() const { return make_unique<GeneratorExpr>(*this); }
void GeneratorExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

DictGeneratorExpr::DictGeneratorExpr(ExprPtr key, ExprPtr expr,
                                     vector<GeneratorBody> &&loops)
    : Expr(), key(move(key)), expr(move(expr)), loops(move(loops)) {}
DictGeneratorExpr::DictGeneratorExpr(const DictGeneratorExpr &expr)
    : Expr(expr), key(ast::clone(expr.key)), expr(ast::clone(expr.expr)),
      loops(ast::clone_nop(expr.loops)) {}
string DictGeneratorExpr::toString() const {
  string s;
  for (auto &i : loops) {
    string q;
    for (auto &k : i.conds)
      q += format(" IF {}", k->toString());
    s += format("FOR ({}) {}{}", i.vars->toString(), i.gen->toString(), q);
  }
  return wrapType(format("DICT_GEN {} {}{}", key->toString(), expr->toString(), s));
}
ExprPtr DictGeneratorExpr::clone() const {
  return make_unique<DictGeneratorExpr>(*this);
}
void DictGeneratorExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

IfExpr::IfExpr(ExprPtr cond, ExprPtr ifexpr, ExprPtr elsexpr)
    : Expr(), cond(move(cond)), ifexpr(move(ifexpr)), elsexpr(move(elsexpr)) {}
IfExpr::IfExpr(const IfExpr &expr)
    : Expr(expr), cond(ast::clone(expr.cond)), ifexpr(ast::clone(expr.ifexpr)),
      elsexpr(ast::clone(expr.elsexpr)) {}
string IfExpr::toString() const {
  return wrapType(
      format("IF {} {} {}", cond->toString(), ifexpr->toString(), elsexpr->toString()));
}
ExprPtr IfExpr::clone() const { return make_unique<IfExpr>(*this); }
void IfExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

UnaryExpr::UnaryExpr(string op, ExprPtr expr)
    : Expr(), op(move(op)), expr(move(expr)) {}
UnaryExpr::UnaryExpr(const UnaryExpr &expr)
    : Expr(expr), op(expr.op), expr(ast::clone(expr.expr)) {}
string UnaryExpr::toString() const {
  return wrapType(format("UNARY '{}' {}", op, expr->toString()));
}
ExprPtr UnaryExpr::clone() const { return make_unique<UnaryExpr>(*this); }
void UnaryExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

BinaryExpr::BinaryExpr(ExprPtr lexpr, string op, ExprPtr rexpr, bool inPlace)
    : Expr(), op(move(op)), lexpr(move(lexpr)), rexpr(move(rexpr)), inPlace(inPlace) {}
BinaryExpr::BinaryExpr(const BinaryExpr &expr)
    : Expr(expr), op(expr.op), lexpr(ast::clone(expr.lexpr)),
      rexpr(ast::clone(expr.rexpr)), inPlace(expr.inPlace) {}
string BinaryExpr::toString() const {
  return wrapType(format("BINARY {} '{}' {}{}", lexpr->toString(), op,
                         rexpr->toString(), inPlace ? " INPLACE" : ""));
}
ExprPtr BinaryExpr::clone() const { return make_unique<BinaryExpr>(*this); }
void BinaryExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

PipeExpr::Pipe PipeExpr::Pipe::clone() const { return {op, ast::clone(expr)}; }

PipeExpr::PipeExpr(vector<PipeExpr::Pipe> &&items) : Expr(), items(move(items)) {}
PipeExpr::PipeExpr(const PipeExpr &expr)
    : Expr(expr), items(ast::clone_nop(expr.items)), inTypes(expr.inTypes) {}
string PipeExpr::toString() const {
  vector<string> s;
  for (auto &i : items)
    s.push_back(format("({}{})", i.expr->toString(), i.op == "||>" ? " PARALLEL" : ""));
  return wrapType(format("PIPE {}", join(s, " ")));
}
ExprPtr PipeExpr::clone() const { return make_unique<PipeExpr>(*this); }
void PipeExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

IndexExpr::IndexExpr(ExprPtr expr, ExprPtr index)
    : Expr(), expr(move(expr)), index(move(index)) {}
IndexExpr::IndexExpr(const IndexExpr &expr)
    : Expr(expr), expr(ast::clone(expr.expr)), index(ast::clone(expr.index)) {}
string IndexExpr::toString() const {
  return wrapType(format("INDEX {} {}", expr->toString(), index->toString()));
}
ExprPtr IndexExpr::clone() const { return make_unique<IndexExpr>(*this); }
void IndexExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

CallExpr::Arg CallExpr::Arg::clone() const { return {name, ast::clone(value)}; }

CallExpr::CallExpr(const CallExpr &expr)
    : Expr(expr), expr(ast::clone(expr.expr)), args(ast::clone_nop(expr.args)) {}
CallExpr::CallExpr(ExprPtr expr, vector<CallExpr::Arg> &&a)
    : Expr(), expr(move(expr)), args(move(a)) {}
CallExpr::CallExpr(ExprPtr expr, vector<ExprPtr> &&exprArgs)
    : Expr(), expr(move(expr)) {
  for (auto &i : exprArgs) {
    args.push_back(CallExpr::Arg{"", move(i)});
  }
}
CallExpr::CallExpr(ExprPtr expr, ExprPtr arg1, ExprPtr arg2, ExprPtr arg3)
    : Expr(), expr(move(expr)) {
  if (arg1)
    args.push_back(CallExpr::Arg{"", move(arg1)});
  if (arg2)
    args.push_back(CallExpr::Arg{"", move(arg2)});
  if (arg3)
    args.push_back(CallExpr::Arg{"", move(arg3)});
}
string CallExpr::toString() const {
  string s;
  for (auto &i : args)
    if (i.name.empty())
      s += " " + i.value->toString();
    else
      s += format(" ({} NAME={})", i.value->toString(), i.name);
  return wrapType(format("CALL {}{}", expr->toString(), s));
}
ExprPtr CallExpr::clone() const { return make_unique<CallExpr>(*this); }
void CallExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

DotExpr::DotExpr(ExprPtr expr, string member)
    : Expr(), expr(move(expr)), member(move(member)) {}
DotExpr::DotExpr(const DotExpr &expr)
    : Expr(expr), expr(ast::clone(expr.expr)), member(expr.member) {}
string DotExpr::toString() const {
  return wrapType(format("DOT {} {}", expr->toString(), member));
}
ExprPtr DotExpr::clone() const { return make_unique<DotExpr>(*this); }
void DotExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

SliceExpr::SliceExpr(ExprPtr start, ExprPtr stop, ExprPtr step)
    : Expr(), start(move(start)), stop(move(stop)), step(move(step)) {}
SliceExpr::SliceExpr(const SliceExpr &expr)
    : Expr(expr), start(ast::clone(expr.start)), stop(ast::clone(expr.stop)),
      step(ast::clone(expr.step)) {}
string SliceExpr::toString() const {
  return wrapType(format("SLICE{}{}{}",
                         start ? format(" START={}", start->toString()) : "",
                         stop ? format(" END={}", stop->toString()) : "",
                         step ? format(" STEP={}", step->toString()) : ""));
}
ExprPtr SliceExpr::clone() const { return make_unique<SliceExpr>(*this); }
void SliceExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

EllipsisExpr::EllipsisExpr(bool isPipeArg) : Expr(), isPipeArg(isPipeArg) {}
string EllipsisExpr::toString() const { return wrapType("ELLIPSIS"); }
ExprPtr EllipsisExpr::clone() const { return make_unique<EllipsisExpr>(*this); }
void EllipsisExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

TypeOfExpr::TypeOfExpr(ExprPtr expr) : Expr(), expr(move(expr)) {}
TypeOfExpr::TypeOfExpr(const TypeOfExpr &e) : Expr(e), expr(ast::clone(e.expr)) {}
string TypeOfExpr::toString() const {
  return wrapType(format("TYPEOF {}", expr->toString()));
}
ExprPtr TypeOfExpr::clone() const { return make_unique<TypeOfExpr>(*this); }
void TypeOfExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

LambdaExpr::LambdaExpr(vector<string> &&vars, ExprPtr expr)
    : Expr(), vars(vars), expr(move(expr)) {}
LambdaExpr::LambdaExpr(const LambdaExpr &expr)
    : Expr(expr), vars(expr.vars), expr(ast::clone(expr.expr)) {}
string LambdaExpr::toString() const {
  return wrapType(format("LAMBDA {} {}", join(vars, " "), expr->toString()));
}
ExprPtr LambdaExpr::clone() const { return make_unique<LambdaExpr>(*this); }
void LambdaExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

YieldExpr::YieldExpr() : Expr() {}
string YieldExpr::toString() const { return "YIELD"; }
ExprPtr YieldExpr::clone() const { return make_unique<YieldExpr>(*this); }
void YieldExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

StmtExpr::StmtExpr(vector<unique_ptr<Stmt>> &&stmts, ExprPtr expr)
    : Expr(), stmts(move(stmts)), expr(move(expr)) {}
StmtExpr::StmtExpr(const StmtExpr &expr)
    : Expr(expr), stmts(ast::clone(expr.stmts)), expr(ast::clone(expr.expr)) {}
string StmtExpr::toString() const {
  return wrapType(format("STMT ({}) {}", combine(stmts, " "), expr->toString()));
}
ExprPtr StmtExpr::clone() const { return make_unique<StmtExpr>(*this); }
void StmtExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

PtrExpr::PtrExpr(ExprPtr expr) : Expr(), expr(move(expr)) {}
PtrExpr::PtrExpr(const PtrExpr &expr) : Expr(expr), expr(ast::clone(expr.expr)) {}
string PtrExpr::toString() const {
  return wrapType(format("PTR {}", expr->toString()));
}
ExprPtr PtrExpr::clone() const { return make_unique<PtrExpr>(*this); }
void PtrExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

TupleIndexExpr::TupleIndexExpr(ExprPtr expr, int index)
    : Expr(), expr(move(expr)), index(index) {}
TupleIndexExpr::TupleIndexExpr(const TupleIndexExpr &expr)
    : Expr(expr), expr(ast::clone(expr.expr)), index(expr.index) {}
string TupleIndexExpr::toString() const {
  return wrapType(format("TUPLE_INDEX {} {}", expr->toString(), index));
}
ExprPtr TupleIndexExpr::clone() const { return make_unique<TupleIndexExpr>(*this); }
void TupleIndexExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

InstantiateExpr::InstantiateExpr(ExprPtr typeExpr, vector<ExprPtr> &&typeParams)
    : Expr(), typeExpr(move(typeExpr)), typeParams(move(typeParams)) {}
InstantiateExpr::InstantiateExpr(ExprPtr typeExpr, ExprPtr typeParam)
    : Expr(), typeExpr(move(typeExpr)) {
  typeParams.push_back(move(typeParam));
}
InstantiateExpr::InstantiateExpr(const InstantiateExpr &expr)
    : Expr(expr), typeExpr(ast::clone(expr.typeExpr)),
      typeParams(ast::clone(expr.typeParams)) {}
string InstantiateExpr::toString() const {
  return wrapType(
      format("INSTANTIATE {} {}", typeExpr->toString(), combine(typeParams)));
}
ExprPtr InstantiateExpr::clone() const { return make_unique<InstantiateExpr>(*this); }
void InstantiateExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

StackAllocExpr::StackAllocExpr(ExprPtr typeExpr, ExprPtr expr)
    : Expr(), typeExpr(move(typeExpr)), expr(move(expr)) {}
StackAllocExpr::StackAllocExpr(const StackAllocExpr &expr)
    : Expr(expr), typeExpr(ast::clone(expr.typeExpr)), expr(ast::clone(expr.expr)) {}
string StackAllocExpr::toString() const {
  return wrapType(format("STACK_ALLOC {} {}", typeExpr->toString(), expr->toString()));
}
ExprPtr StackAllocExpr::clone() const { return make_unique<StackAllocExpr>(*this); }
void StackAllocExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

StaticExpr::StaticExpr(ExprPtr expr, set<string> &&c)
    : Expr(), expr(move(expr)), captures(c) {}
StaticExpr::StaticExpr(const StaticExpr &expr)
    : Expr(expr), expr(ast::clone(expr.expr)), captures(expr.captures) {}
string StaticExpr::toString() const {
  return wrapType(format("STATIC {}", expr->toString()));
}
ExprPtr StaticExpr::clone() const { return make_unique<StaticExpr>(*this); }
void StaticExpr::accept(ASTVisitor &visitor) const { visitor.visit(this); }

} // namespace ast
} // namespace seq
