#include <memory>
#include <string>
#include <vector>

#include "parser/ast/ast/pattern.h"

using fmt::format;
using std::move;

namespace seq {
namespace ast {

Pattern::Pattern() : type(nullptr) {}
Pattern::Pattern(const Pattern &e) : seq::SrcObject(e), type(e.type) {}

StarPattern::StarPattern() : Pattern() {}
StarPattern::StarPattern(const StarPattern &p) : Pattern(p) {}
string StarPattern::toString() const { return "(STAR)"; }

IntPattern::IntPattern(int v) : Pattern(), value(v) {}
IntPattern::IntPattern(const IntPattern &p) : Pattern(p), value(p.value) {}
string IntPattern::toString() const { return format("(INT {})", value); }

BoolPattern::BoolPattern(bool v) : Pattern(), value(v) {}
BoolPattern::BoolPattern(const BoolPattern &p) : Pattern(p), value(p.value) {}
string BoolPattern::toString() const { return format("(BOOL {})", value); }

StrPattern::StrPattern(string v) : Pattern(), value(v) {}
StrPattern::StrPattern(StrPattern p) : Pattern(p), value(p.value) {}
string StrPattern::toString() const { return format("(STR '{}')", escape(value)); }

RangePattern::RangePattern(int s, int e) : Pattern(), start(s), end(e) {}
RangePattern::RangePattern(const RangePattern &p)
    : Pattern(p), start(p.start), end(p.end) {}
string RangePattern::toString() const { return format("(RANGE {} {})", start, end); }

TuplePattern::TuplePattern(vector<PatternPtr> &&p) : Pattern(), patterns(move(p)) {}
TuplePattern::TuplePattern(const TuplePattern &p)
    : Pattern(p), patterns(ast::clone(p.patterns)) {}
string TuplePattern::toString() const {
  return format("(TUPLE {})", combine(patterns));
}

ListPattern::ListPattern(vector<PatternPtr> &&p) : Pattern(), patterns(move(p)) {}
ListPattern::ListPattern(const ListPattern &p)
    : Pattern(p), patterns(ast::clone(p.patterns)) {}
string ListPattern::toString() const { return format("(LIST {})", combine(patterns)); }

OrPattern::OrPattern(vector<PatternPtr> &&p) : Pattern(), patterns(move(p)) {}
OrPattern::OrPattern(const OrPattern &p)
    : Pattern(p), patterns(ast::clone(p.patterns)) {}
string OrPattern::toString() const { return format("(OR {})", combine(patterns)); }

WildcardPattern::WildcardPattern(string v) : Pattern(), var(v) {}
WildcardPattern::WildcardPattern(const WildcardPattern &p) : Pattern(p), var(p.var) {}
string WildcardPattern::toString() const {
  return var == "" ? "(WILD)" : format("(WILD {})", var);
}

GuardedPattern::GuardedPattern(PatternPtr p, ExprPtr c)
    : Pattern(), pattern(move(p)), cond(move(c)) {}
GuardedPattern::GuardedPattern(const GuardedPattern &p)
    : Pattern(p), pattern(ast::clone(p.pattern)), cond(ast::clone(p.cond)) {}
string GuardedPattern::toString() const {
  return format("(GUARD {} {})", *pattern, *cond);
}

BoundPattern::BoundPattern(string v, PatternPtr p)
    : Pattern(), var(v), pattern(move(p)) {}
BoundPattern::BoundPattern(const BoundPattern &p)
    : Pattern(p), var(p.var), pattern(ast::clone(p.pattern)) {}
string BoundPattern::toString() const { return format("(BOUND {} {})", var, *pattern); }

} // namespace ast
} // namespace seq
