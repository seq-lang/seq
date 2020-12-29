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

#define ACCEPT_IMPL(T, X)                                                              \
  PatternPtr T::clone() const { return make_unique<T>(*this); }                        \
  void T::accept(X &visitor) { visitor.visit(this); }

using fmt::format;
using std::move;

namespace seq {
namespace ast {

Pattern::Pattern() : type(nullptr), done(false) {}
types::TypePtr Pattern::getType() const { return type; }
void Pattern::setType(types::TypePtr t) { this->type = move(t); }

string StarPattern::toString() const { return "(STAR)"; }
ACCEPT_IMPL(StarPattern, ASTVisitor);

IntPattern::IntPattern(int value) : Pattern(), value(value) {}
string IntPattern::toString() const { return format("(INT {})", value); }
ACCEPT_IMPL(IntPattern, ASTVisitor);

BoolPattern::BoolPattern(bool value) : Pattern(), value(value) {}
string BoolPattern::toString() const { return format("(BOOL {})", value); }
ACCEPT_IMPL(BoolPattern, ASTVisitor);

StrPattern::StrPattern(string value, string prefix)
    : Pattern(), value(move(value)), prefix(move(prefix)) {}
string StrPattern::toString() const {
  return format("(STR '{}' PREFIX={})", escape(value), prefix);
}
ACCEPT_IMPL(StrPattern, ASTVisitor);

RangePattern::RangePattern(int start, int stop) : Pattern(), start(start), stop(stop) {}
string RangePattern::toString() const { return format("(RANGE {} {})", start, stop); }
ACCEPT_IMPL(RangePattern, ASTVisitor);

TuplePattern::TuplePattern(vector<PatternPtr> &&patterns)
    : Pattern(), patterns(move(patterns)) {}
TuplePattern::TuplePattern(const TuplePattern &pattern)
    : Pattern(pattern), patterns(ast::clone(pattern.patterns)) {}
string TuplePattern::toString() const {
  return format("(TUPLE {})", combine(patterns));
}
ACCEPT_IMPL(TuplePattern, ASTVisitor);

ListPattern::ListPattern(vector<PatternPtr> &&patterns)
    : Pattern(), patterns(move(patterns)) {}
ListPattern::ListPattern(const ListPattern &pattern)
    : Pattern(pattern), patterns(ast::clone(pattern.patterns)) {}
string ListPattern::toString() const { return format("(LIST {})", combine(patterns)); }
ACCEPT_IMPL(ListPattern, ASTVisitor);

OrPattern::OrPattern(vector<PatternPtr> &&patterns)
    : Pattern(), patterns(move(patterns)) {}
OrPattern::OrPattern(const OrPattern &pattern)
    : Pattern(pattern), patterns(ast::clone(pattern.patterns)) {}
string OrPattern::toString() const { return format("(OR {})", combine(patterns)); }
ACCEPT_IMPL(OrPattern, ASTVisitor);

WildcardPattern::WildcardPattern(string var) : Pattern(), var(move(var)) {}
string WildcardPattern::toString() const {
  return var.empty() ? "(WILD)" : format("(WILD {})", var);
}
ACCEPT_IMPL(WildcardPattern, ASTVisitor);

GuardedPattern::GuardedPattern(PatternPtr pattern, ExprPtr cond)
    : Pattern(), pattern(move(pattern)), cond(move(cond)) {}
GuardedPattern::GuardedPattern(const GuardedPattern &pattern)
    : Pattern(pattern), pattern(ast::clone(pattern.pattern)),
      cond(ast::clone(pattern.cond)) {}
string GuardedPattern::toString() const {
  return format("(GUARD {} {})", pattern->toString(), cond->toString());
}
ACCEPT_IMPL(GuardedPattern, ASTVisitor);

BoundPattern::BoundPattern(string var, PatternPtr pattern)
    : Pattern(), var(move(var)), pattern(move(pattern)) {}
BoundPattern::BoundPattern(const BoundPattern &pattern)
    : Pattern(pattern), var(pattern.var), pattern(ast::clone(pattern.pattern)) {}
string BoundPattern::toString() const {
  return format("(BOUND {} {})", var, pattern->toString());
}
ACCEPT_IMPL(BoundPattern, ASTVisitor);

} // namespace ast
} // namespace seq
