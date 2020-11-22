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
PatternPtr StarPattern::clone() const { return make_unique<StarPattern>(*this); }

IntPattern::IntPattern(int v) : Pattern(), value(v) {}
IntPattern::IntPattern(const IntPattern &p) : Pattern(p), value(p.value) {}
string IntPattern::toString() const { return format("(INT {})", value); }
PatternPtr IntPattern::clone() const { return make_unique<IntPattern>(*this); }

BoolPattern::BoolPattern(bool v) : Pattern(), value(v) {}
BoolPattern::BoolPattern(const BoolPattern &p) : Pattern(p), value(p.value) {}
string BoolPattern::toString() const { return format("(BOOL {})", value); }
PatternPtr BoolPattern::clone() const { return make_unique<BoolPattern>(*this); }

StrPattern::StrPattern(string v, string p) : Pattern(), value(v), prefix(p) {}
StrPattern::StrPattern(const StrPattern &p)
    : Pattern(p), value(p.value), prefix(p.prefix) {}
string StrPattern::toString() const {
  return format("(STR '{}' PREFIX={})", escape(value), prefix);
}
PatternPtr StrPattern::clone() const { return make_unique<StrPattern>(*this); }

RangePattern::RangePattern(int s, int e) : Pattern(), start(s), end(e) {}
RangePattern::RangePattern(const RangePattern &p)
    : Pattern(p), start(p.start), end(p.end) {}
string RangePattern::toString() const { return format("(RANGE {} {})", start, end); }
PatternPtr RangePattern::clone() const { return make_unique<RangePattern>(*this); }

TuplePattern::TuplePattern(vector<PatternPtr> &&p) : Pattern(), patterns(move(p)) {}
TuplePattern::TuplePattern(const TuplePattern &p)
    : Pattern(p), patterns(ast::clone(p.patterns)) {}
string TuplePattern::toString() const {
  return format("(TUPLE {})", combine(patterns));
}
PatternPtr TuplePattern::clone() const { return make_unique<TuplePattern>(*this); }

ListPattern::ListPattern(vector<PatternPtr> &&p) : Pattern(), patterns(move(p)) {}
ListPattern::ListPattern(const ListPattern &p)
    : Pattern(p), patterns(ast::clone(p.patterns)) {}
string ListPattern::toString() const { return format("(LIST {})", combine(patterns)); }
PatternPtr ListPattern::clone() const { return make_unique<ListPattern>(*this); }

OrPattern::OrPattern(vector<PatternPtr> &&p) : Pattern(), patterns(move(p)) {}
OrPattern::OrPattern(const OrPattern &p)
    : Pattern(p), patterns(ast::clone(p.patterns)) {}
string OrPattern::toString() const { return format("(OR {})", combine(patterns)); }
PatternPtr OrPattern::clone() const { return make_unique<OrPattern>(*this); }

WildcardPattern::WildcardPattern(string v) : Pattern(), var(v) {}
WildcardPattern::WildcardPattern(const WildcardPattern &p) : Pattern(p), var(p.var) {}
string WildcardPattern::toString() const {
  return var == "" ? "(WILD)" : format("(WILD {})", var);
}
PatternPtr WildcardPattern::clone() const {
  return make_unique<WildcardPattern>(*this);
}

GuardedPattern::GuardedPattern(PatternPtr p, ExprPtr c)
    : Pattern(), pattern(move(p)), cond(move(c)) {}
GuardedPattern::GuardedPattern(const GuardedPattern &p)
    : Pattern(p), pattern(ast::clone(p.pattern)), cond(ast::clone(p.cond)) {}
string GuardedPattern::toString() const {
  return format("(GUARD {} {})", *pattern, *cond);
}
PatternPtr GuardedPattern::clone() const { return make_unique<GuardedPattern>(*this); }

BoundPattern::BoundPattern(string v, PatternPtr p)
    : Pattern(), var(v), pattern(move(p)) {}
BoundPattern::BoundPattern(const BoundPattern &p)
    : Pattern(p), var(p.var), pattern(ast::clone(p.pattern)) {}
string BoundPattern::toString() const { return format("(BOUND {} {})", var, *pattern); }
PatternPtr BoundPattern::clone() const { return make_unique<BoundPattern>(*this); }

} // namespace ast
} // namespace seq
