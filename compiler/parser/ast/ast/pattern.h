/**
 * ast.h
 * Describes Seq AST.
 */

#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/ast/expr.h"
#include "parser/ast/ast/visitor.h"
#include "parser/ast/types.h"
#include "parser/common.h"

namespace seq {
namespace ast {

struct Pattern : public seq::SrcObject {
  types::TypePtr type;

  Pattern();
  Pattern(const Pattern &e);
  virtual ~Pattern() {}
  virtual unique_ptr<Pattern> clone() const = 0;

  /// Convert node to a string
  virtual string toString() const = 0;
  /// Accept an AST walker/visitor
  virtual void accept(ASTVisitor &) const = 0;

  types::TypePtr getType() const { return type; }
  void setType(types::TypePtr t) { type = t; }

  /// Allow pretty-printing to C++ streams
  friend std::ostream &operator<<(std::ostream &out, const Pattern &c) {
    return out << c.toString();
  }
};
typedef unique_ptr<Pattern> PatternPtr;

struct StarPattern : public Pattern {
  StarPattern();
  StarPattern(const StarPattern &p);

  string toString() const override;
  NODE_UTILITY(Pattern, StarPattern);
};

struct IntPattern : public Pattern {
  int value;

  IntPattern(int v);
  IntPattern(const IntPattern &p);

  string toString() const override;
  NODE_UTILITY(Pattern, IntPattern);
};

struct BoolPattern : public Pattern {
  bool value;

  BoolPattern(bool v);
  BoolPattern(const BoolPattern &p);

  string toString() const override;
  NODE_UTILITY(Pattern, BoolPattern);
};

struct StrPattern : public Pattern {
  string prefix;
  string value;

  StrPattern(string p, string v);
  StrPattern(const StrPattern &p);

  string toString() const override;
  NODE_UTILITY(Pattern, StrPattern);
};

struct RangePattern : public Pattern {
  int start, end;

  RangePattern(int s, int e);
  RangePattern(const RangePattern &p);

  string toString() const override;
  NODE_UTILITY(Pattern, RangePattern);
};

struct TuplePattern : public Pattern {
  vector<PatternPtr> patterns;

  TuplePattern(vector<PatternPtr> &&p);
  TuplePattern(const TuplePattern &p);

  string toString() const override;
  NODE_UTILITY(Pattern, TuplePattern);
};

struct ListPattern : public Pattern {
  vector<PatternPtr> patterns;

  ListPattern(vector<PatternPtr> &&p);
  ListPattern(const ListPattern &p);

  string toString() const override;
  NODE_UTILITY(Pattern, ListPattern);
};

struct OrPattern : public Pattern {
  vector<PatternPtr> patterns;

  OrPattern(vector<PatternPtr> &&p);
  OrPattern(const OrPattern &p);

  string toString() const override;
  NODE_UTILITY(Pattern, OrPattern);
};

struct WildcardPattern : public Pattern {
  string var;

  WildcardPattern(string v);
  WildcardPattern(const WildcardPattern &p);

  string toString() const override;
  NODE_UTILITY(Pattern, WildcardPattern);
};

struct GuardedPattern : public Pattern {
  PatternPtr pattern;
  ExprPtr cond;

  GuardedPattern(PatternPtr p, ExprPtr c);
  GuardedPattern(const GuardedPattern &p);

  string toString() const override;
  NODE_UTILITY(Pattern, GuardedPattern);
};

struct BoundPattern : public Pattern {
  string var;
  PatternPtr pattern;

  BoundPattern(string v, PatternPtr p);
  BoundPattern(const BoundPattern &p);

  string toString() const override;
  NODE_UTILITY(Pattern, BoundPattern);
};

} // namespace ast
} // namespace seq
