/*
 * pattern.cpp --- Seq AST match-case patterns.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <memory>
#include <vector>

#include "parser/ast.h"
#include "parser/visitors/visitor.h"

using fmt::format;
using std::move;

namespace seq {
namespace ast {

Pattern::Pattern() : type(nullptr) {}
types::TypePtr Pattern::getType() const { return type; }
void Pattern::setType(types::TypePtr t) { this->type = move(t); }

string StarPattern::toString() const { return "(STAR)"; }
PatternPtr StarPattern::clone() const { return make_unique<StarPattern>(*this); }
void StarPattern::accept(ASTVisitor &visitor) const { visitor.visit(this); }

IntPattern::IntPattern(int value) : Pattern(), value(value) {}
string IntPattern::toString() const { return format("(INT {})", value); }
PatternPtr IntPattern::clone() const { return make_unique<IntPattern>(*this); }
void IntPattern::accept(ASTVisitor &visitor) const { visitor.visit(this); }

BoolPattern::BoolPattern(bool value) : Pattern(), value(value) {}
string BoolPattern::toString() const { return format("(BOOL {})", value); }
PatternPtr BoolPattern::clone() const { return make_unique<BoolPattern>(*this); }
void BoolPattern::accept(ASTVisitor &visitor) const { visitor.visit(this); }

StrPattern::StrPattern(string value, string prefix)
    : Pattern(), value(move(value)), prefix(move(prefix)) {}
string StrPattern::toString() const {
  return format("(STR '{}' PREFIX={})", escape(value), prefix);
}
PatternPtr StrPattern::clone() const { return make_unique<StrPattern>(*this); }
void StrPattern::accept(ASTVisitor &visitor) const { visitor.visit(this); }

RangePattern::RangePattern(int start, int stop) : Pattern(), start(start), stop(stop) {}
string RangePattern::toString() const { return format("(RANGE {} {})", start, stop); }
PatternPtr RangePattern::clone() const { return make_unique<RangePattern>(*this); }
void RangePattern::accept(ASTVisitor &visitor) const { visitor.visit(this); }

TuplePattern::TuplePattern(vector<PatternPtr> &&patterns)
    : Pattern(), patterns(move(patterns)) {}
TuplePattern::TuplePattern(const TuplePattern &pattern)
    : Pattern(pattern), patterns(ast::clone(pattern.patterns)) {}
string TuplePattern::toString() const {
  return format("(TUPLE {})", combine(patterns));
}
PatternPtr TuplePattern::clone() const { return make_unique<TuplePattern>(*this); }
void TuplePattern::accept(ASTVisitor &visitor) const { visitor.visit(this); }

ListPattern::ListPattern(vector<PatternPtr> &&patterns)
    : Pattern(), patterns(move(patterns)) {}
ListPattern::ListPattern(const ListPattern &pattern)
    : Pattern(pattern), patterns(ast::clone(pattern.patterns)) {}
string ListPattern::toString() const { return format("(LIST {})", combine(patterns)); }
PatternPtr ListPattern::clone() const { return make_unique<ListPattern>(*this); }
void ListPattern::accept(ASTVisitor &visitor) const { visitor.visit(this); }

OrPattern::OrPattern(vector<PatternPtr> &&patterns)
    : Pattern(), patterns(move(patterns)) {}
OrPattern::OrPattern(const OrPattern &pattern)
    : Pattern(pattern), patterns(ast::clone(pattern.patterns)) {}
string OrPattern::toString() const { return format("(OR {})", combine(patterns)); }
PatternPtr OrPattern::clone() const { return make_unique<OrPattern>(*this); }
void OrPattern::accept(ASTVisitor &visitor) const { visitor.visit(this); }

WildcardPattern::WildcardPattern(string var) : Pattern(), var(move(var)) {}
string WildcardPattern::toString() const {
  return var.empty() ? "(WILD)" : format("(WILD {})", var);
}
PatternPtr WildcardPattern::clone() const {
  return make_unique<WildcardPattern>(*this);
}
void WildcardPattern::accept(ASTVisitor &visitor) const { visitor.visit(this); }

GuardedPattern::GuardedPattern(PatternPtr pattern, ExprPtr cond)
    : Pattern(), pattern(move(pattern)), cond(move(cond)) {}
GuardedPattern::GuardedPattern(const GuardedPattern &pattern)
    : Pattern(pattern), pattern(ast::clone(pattern.pattern)),
      cond(ast::clone(pattern.cond)) {}
string GuardedPattern::toString() const {
  return format("(GUARD {} {})", pattern->toString(), cond->toString());
}
PatternPtr GuardedPattern::clone() const { return make_unique<GuardedPattern>(*this); }
void GuardedPattern::accept(ASTVisitor &visitor) const { visitor.visit(this); }

BoundPattern::BoundPattern(string var, PatternPtr pattern)
    : Pattern(), var(move(var)), pattern(move(pattern)) {}
BoundPattern::BoundPattern(const BoundPattern &pattern)
    : Pattern(pattern), var(pattern.var), pattern(ast::clone(pattern.pattern)) {}
string BoundPattern::toString() const {
  return format("(BOUND {} {})", var, pattern->toString());
}
PatternPtr BoundPattern::clone() const { return make_unique<BoundPattern>(*this); }
void BoundPattern::accept(ASTVisitor &visitor) const { visitor.visit(this); }

} // namespace ast
} // namespace seq
