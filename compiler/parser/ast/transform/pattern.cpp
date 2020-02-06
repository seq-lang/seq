#include "util/fmt/format.h"
#include "util/fmt/ostream.h"
#include <memory>
#include <ostream>
#include <stack>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/ast/expr.h"
#include "parser/ast/stmt.h"
#include "parser/ast/transform/expr.h"
#include "parser/ast/transform/pattern.h"
#include "parser/ast/visitor.h"
#include "parser/common.h"
#include "parser/context.h"
#include "parser/ocaml.h"

using fmt::format;
using std::get;
using std::make_unique;
using std::move;
using std::ostream;
using std::stack;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::unordered_set;
using std::vector;

#define RETURN(T, ...)                                                         \
  (this->result = fwdSrcInfo(make_unique<T>(__VA_ARGS__), pat->getSrcInfo()))

TransformPatternVisitor::TransformPatternVisitor(
    TransformStmtVisitor &stmtVisitor)
    : stmtVisitor(stmtVisitor) {}

PatternPtr TransformPatternVisitor::transform(const Pattern *ptr) {
  TransformPatternVisitor v(stmtVisitor);
  ptr->accept(v);
  return move(v.result);
}

vector<PatternPtr>
TransformPatternVisitor::transform(const vector<PatternPtr> &pats) {
  vector<PatternPtr> r;
  for (auto &e : pats) {
    r.push_back(transform(e));
  }
  return r;
}

void TransformPatternVisitor::visit(const StarPattern *pat) {
  RETURN(StarPattern, );
}

void TransformPatternVisitor::visit(const IntPattern *pat) {
  RETURN(IntPattern, pat->value);
}

void TransformPatternVisitor::visit(const BoolPattern *pat) {
  RETURN(BoolPattern, pat->value);
}

void TransformPatternVisitor::visit(const StrPattern *pat) {
  RETURN(StrPattern, pat->value);
}

void TransformPatternVisitor::visit(const SeqPattern *pat) {
  RETURN(SeqPattern, pat->value);
}

void TransformPatternVisitor::visit(const RangePattern *pat) {
  RETURN(RangePattern, pat->start, pat->end);
}

void TransformPatternVisitor::visit(const TuplePattern *pat) {
  RETURN(TuplePattern, transform(pat->patterns));
}

void TransformPatternVisitor::visit(const ListPattern *pat) {
  RETURN(ListPattern, transform(pat->patterns));
}

void TransformPatternVisitor::visit(const OrPattern *pat) {
  RETURN(OrPattern, transform(pat->patterns));
}

void TransformPatternVisitor::visit(const WildcardPattern *pat) {
  RETURN(WildcardPattern, pat->var);
}

void TransformPatternVisitor::visit(const GuardedPattern *pat) {
  RETURN(GuardedPattern, transform(pat->pattern),
         stmtVisitor.transform(pat->cond));
}

void TransformPatternVisitor::visit(const BoundPattern *pat) {
  RETURN(BoundPattern, pat->var, transform(pat->pattern));
}
