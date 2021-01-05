/*
 * pattern.h --- Seq AST match-case patterns.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/expr.h"
#include "parser/ast/types.h"
#include "parser/common.h"

namespace seq {
namespace ast {

#define ACCEPT(X)                                                                      \
  PatternPtr clone() const override;                                                   \
  void accept(X &visitor) override

// Forward declarations
struct ASTVisitor;

/**
 * A Seq AST match-case pattern.
 * Each AST pattern owns its children and is intended to be instantiated as a
 * unique_ptr.
 */
struct Pattern : public seq::SrcObject {
  /// Type of the expression. nullptr by default.
  types::TypePtr type;
  bool done;

  Pattern();
  Pattern(const Pattern &e) = default;

  /// Convert a node to an S-expression.
  virtual string toString() const = 0;
  /// Deep copy a node.
  virtual unique_ptr<Pattern> clone() const = 0;
  /// Accept an AST visitor.
  virtual void accept(ASTVisitor &visitor) = 0;

  /// Get a node type.
  /// @return Type pointer or a nullptr if a type is not set.
  types::TypePtr getType() const;
  /// Set a node type.
  void setType(types::TypePtr type);

  /// Allow pretty-printing to C++ streams.
  friend std::ostream &operator<<(std::ostream &out, const Pattern &pattern) {
    return out << pattern.toString();
  }
};
using PatternPtr = unique_ptr<Pattern>;

/// Star pattern (...).
/// @example case [a, ..., b]
struct StarPattern : public Pattern {
  StarPattern() = default;
  StarPattern(const StarPattern &pattern) = default;

  string toString() const override;
  ACCEPT(ASTVisitor);
};

/// Int pattern (value).
/// @example case 1
/// TODO: Use string-based IntExpr and parse the integer in simplify stage.
struct IntPattern : public Pattern {
  int value;

  explicit IntPattern(int value);
  IntPattern(const IntPattern &pattern) = default;

  string toString() const override;
  ACCEPT(ASTVisitor);
};

/// Bool pattern (value).
/// @example case True
struct BoolPattern : public Pattern {
  bool value;

  explicit BoolPattern(bool value);
  BoolPattern(const BoolPattern &pattern) = default;

  string toString() const override;
  ACCEPT(ASTVisitor);
};

/// String pattern (prefix"value").
/// @example case "a"
/// @example case s"ACGT"
struct StrPattern : public Pattern {
  string value;
  string prefix;

  explicit StrPattern(string value, string prefix = "");
  StrPattern(const StrPattern &pattern) = default;

  string toString() const override;
  ACCEPT(ASTVisitor);
};

/// Range pattern (start...stop).
/// @example case 1...5
struct RangePattern : public Pattern {
  int start, stop;

  RangePattern(int start, int stop);
  RangePattern(const RangePattern &pattern) = default;

  string toString() const override;
  ACCEPT(ASTVisitor);
};

/// Tuple pattern ((patterns...)).
/// @example case (a, 1)
struct TuplePattern : public Pattern {
  vector<PatternPtr> patterns;

  explicit TuplePattern(vector<PatternPtr> &&patterns);
  TuplePattern(const TuplePattern &pattern);

  string toString() const override;
  ACCEPT(ASTVisitor);
};

/// List pattern ([patterns...]).
/// @example case [a, ..., b]
struct ListPattern : public Pattern {
  vector<PatternPtr> patterns;

  explicit ListPattern(vector<PatternPtr> &&patterns);
  ListPattern(const ListPattern &pattern);

  string toString() const override;
  ACCEPT(ASTVisitor);
};

/// Or pattern (pattern or pattern or ...).
/// @example case a or 1
struct OrPattern : public Pattern {
  vector<PatternPtr> patterns;

  explicit OrPattern(vector<PatternPtr> &&patterns);
  OrPattern(const OrPattern &pattern);

  string toString() const override;
  ACCEPT(ASTVisitor);
};

/// Wildcard pattern (var).
/// @example case a
/// @example case _
struct WildcardPattern : public Pattern {
  string var;

  explicit WildcardPattern(string var);
  WildcardPattern(const WildcardPattern &pattern) = default;

  string toString() const override;
  ACCEPT(ASTVisitor);
};

/// Conditional-guarded (conditional) pattern (pattern if cond).
/// @example case a if a % 2 == 1
struct GuardedPattern : public Pattern {
  PatternPtr pattern;
  ExprPtr cond;

  GuardedPattern(PatternPtr pattern, ExprPtr cond);
  GuardedPattern(const GuardedPattern &pattern);

  string toString() const override;
  ACCEPT(ASTVisitor);
};

/// Variable-bound pattern (pattern as var).
/// @example case (a, 1) as v
struct BoundPattern : public Pattern {
  string var;
  PatternPtr pattern;

  BoundPattern(string var, PatternPtr pattern);
  BoundPattern(const BoundPattern &pattern);

  string toString() const override;
  ACCEPT(ASTVisitor);
};

#undef ACCEPT

} // namespace ast
} // namespace seq
