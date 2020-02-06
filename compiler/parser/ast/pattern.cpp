#include <memory>
#include <ostream>
#include <string>
#include <tuple>
#include <vector>

#include "util/fmt/format.h"
#include "util/fmt/ostream.h"

#include "parser/ast/pattern.h"
#include "parser/common.h"

using fmt::format;
using std::move;
using std::ostream;
using std::string;
using std::tuple;
using std::unique_ptr;
using std::vector;

StarPattern::StarPattern() {}
string StarPattern::to_string() const { return "#star"; }

IntPattern::IntPattern(int v) : value(v) {}
string IntPattern::to_string() const { return format("(#int {})", value); }

BoolPattern::BoolPattern(bool v) : value(v) {}
string BoolPattern::to_string() const { return format("(#bool {})", value); }

StrPattern::StrPattern(string v) : value(v) {}
string StrPattern::to_string() const {
  return format("(#str '{}')", escape(value));
}

SeqPattern::SeqPattern(string v) : value(v) {}
string SeqPattern::to_string() const {
  return format("(#seq '{}')", escape(value));
}

RangePattern::RangePattern(int s, int e) : start(s), end(e) {}
string RangePattern::to_string() const {
  return format("(#range {} {})", start, end);
}

TuplePattern::TuplePattern(vector<PatternPtr> p) : patterns(move(p)) {}
string TuplePattern::to_string() const {
  return format("(#tuple {})", combine(patterns));
}

ListPattern::ListPattern(vector<PatternPtr> p) : patterns(move(p)) {}
string ListPattern::to_string() const {
  return format("(#list {})", combine(patterns));
}

OrPattern::OrPattern(vector<PatternPtr> p) : patterns(move(p)) {}
string OrPattern::to_string() const {
  return format("(#or {})", combine(patterns));
}

WildcardPattern::WildcardPattern(string v) : var(v) {}
string WildcardPattern::to_string() const {
  return var == "" ? "#wild" : format("(#wild {})", var);
}

GuardedPattern::GuardedPattern(PatternPtr p, ExprPtr c)
    : pattern(move(p)), cond(move(c)) {}
string GuardedPattern::to_string() const {
  return format("(#guard {} {})", *pattern, *cond);
}

BoundPattern::BoundPattern(string v, PatternPtr p) : var(v), pattern(move(p)) {}
string BoundPattern::to_string() const {
  return format("(#bound {} {})", var, *pattern);
}
