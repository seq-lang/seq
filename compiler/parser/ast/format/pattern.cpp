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
#include "parser/ast/format/expr.h"
#include "parser/ast/format/pattern.h"
#include "parser/ast/stmt.h"
#include "parser/ast/visitor.h"
#include "parser/common.h"
#include "parser/context.h"
#include "parser/ocaml.h"

using fmt::format;
using std::get;
using std::move;
using std::ostream;
using std::stack;
using std::string;
using std::vector;

#define RETURN(T, ...)                                                         \
  this->result = fmt::format(T, __VA_ARGS__);                                  \
  return

namespace seq {
namespace ast {

string FormatPatternVisitor::transform(const Pattern *ptr) {
  FormatPatternVisitor v;
  ptr->accept(v);
  return v.result;
}

void FormatPatternVisitor::visit(const StarPattern *pat) {
  this->result = "...";
}

void FormatPatternVisitor::visit(const IntPattern *pat) {
  RETURN("{}", pat->value);
}

void FormatPatternVisitor::visit(const BoolPattern *pat) {
  RETURN("{}", pat->value ? "True" : "False");
}

void FormatPatternVisitor::visit(const StrPattern *pat) {
  RETURN("\"{}\"", escape(pat->value));
}

void FormatPatternVisitor::visit(const SeqPattern *pat) {
  RETURN("s\"{}\"", escape(pat->value));
}

void FormatPatternVisitor::visit(const RangePattern *pat) {
  RETURN("{} ... {}", pat->start, pat->end);
}

void FormatPatternVisitor::visit(const TuplePattern *pat) {
  string r;
  for (auto &e : pat->patterns) {
    r += transform(e) + ", ";
  }
  RETURN("({})", r);
}

void FormatPatternVisitor::visit(const ListPattern *pat) {
  string r;
  for (auto &e : pat->patterns) {
    r += transform(e) + ", ";
  }
  RETURN("[{}}]", r);
}

void FormatPatternVisitor::visit(const OrPattern *pat) {
  vector<string> r;
  for (auto &e : pat->patterns) {
    r.push_back(format("({})", transform(e)));
  }
  RETURN("{}", fmt::join(r, " or "));
}

void FormatPatternVisitor::visit(const WildcardPattern *pat) {
  RETURN("{}", pat->var == "" ? "_" : pat->var);
}

void FormatPatternVisitor::visit(const GuardedPattern *pat) {
  RETURN("{} if {}", transform(pat->pattern),
         FormatExprVisitor().transform(pat->cond));
}

void FormatPatternVisitor::visit(const BoundPattern *pat) {
  RETURN("({}) as {}", transform(pat->pattern), pat->var);
}

} // namespace ast
} // namespace seq
