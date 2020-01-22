#include <fmt/format.h>
#include <fmt/ostream.h>
#include <memory>
#include <ostream>
#include <stack>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/ast/codegen/expr.h"
#include "parser/ast/expr.h"
#include "parser/ast/stmt.h"
#include "parser/ast/visitor.h"
#include "parser/common.h"
#include "parser/context.h"
#include "seq/seq.h"

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
  this->result = new T(__VA_ARGS__);                                           \
  return
#define ERROR(...) error(expr->getSrcInfo(), __VA_ARGS__)

CodegenExprVisitor::CodegenExprVisitor(Context &ctx,
                                       CodegenExprVisitor &exprVisitor);
    : ctx(ctx), stmtVisitor(stmtVisitor), result(nullptr), captures(captures) {}

PatternPtr CodegenPatternVisitor::transform(const Pattern *ptr) {
  CodegenPatternVisitor v;
  ptr->accept(v);
  return move(v.result);
}

void CodegenPatternVisitor::visit(const StarPattern *pat) {
  RETURN(StarPattern, );
}

void CodegenPatternVisitor::visit(const IntPattern *pat) {
  RETURN(seq::IntPattern, pat->value);
}

void CodegenPatternVisitor::visit(const BoolPattern *pat) {
  RETURN(seq::BoolPattern, pat->value);
}

void CodegenPatternVisitor::visit(const StrPattern *pat) {
  RETURN(seq::StrPattern, pat->value);
}

void CodegenPatternVisitor::visit(const SeqPattern *pat) {
  RETURN(seq::SeqPattern, pat->value);
}

void CodegenPatternVisitor::visit(const RangePattern *pat) {
  RETURN(seq::RangePattern, pat->start, pat->end);
}

void CodegenPatternVisitor::visit(const TuplePattern *pat) {
  vector<seq::Pattern*> result;
  for (auto &p: pat->items) {
    result.push_back(transform(p));
  }
  RETURN(seq::TuplePattern, result);
}
void CodegenPatternVisitor::visit(const ListPattern *pat) {
  vector<seq::Pattern*> result;
  for (auto &p: pat->items) {
    result.push_back(transform(p));
  }
  RETURN(seq::ArrayPattern, result);
}
void CodegenPatternVisitor::visit(const OrPattern *pat) {
  vector<seq::Pattern*> result;
  for (auto &p: pat->items) {
    result.push_back(transform(p));
  }
  RETURN(seq::OrPattern, result);
}
void CodegenPatternVisitor::visit(const GuardedPattern *pat) {
  error("todo");
  // TransformExprVisitor v;
  // pat->expr.accept(v);
  // RETURN(GuardedPattern, transform(pat->pattern), move(v.result));
}
void CodegenPatternVisitor::visit(const BoundPattern *pat) {
  ERROR("unexpected bound pattern");
}
