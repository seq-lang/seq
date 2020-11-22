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
  virtual void accept(ASTVisitor &visitor) const = 0;

  types::TypePtr getType() const { return type; }
  void setType(types::TypePtr t) { type = t; }

  /// Allow pretty-printing to C++ streams
  friend std::ostream &operator<<(std::ostream &out, const Pattern &c) {
    return out << c.toString();
  }
};
using PatternPtr = unique_ptr<Pattern>;

struct StarPattern : public Pattern {
  StarPattern();
  StarPattern(const StarPattern &p);

  string toString() const override;
  PatternPtr clone() const override;
  virtual void accept(ASTVisitor &visitor) const override { visitor.visit(this); }
};

struct IntPattern : public Pattern {
  int value;

  IntPattern(int v);
  IntPattern(const IntPattern &p);

  string toString() const override;
  PatternPtr clone() const override;
  virtual void accept(ASTVisitor &visitor) const override { visitor.visit(this); }
};

struct BoolPattern : public Pattern {
  bool value;

  BoolPattern(bool v);
  BoolPattern(const BoolPattern &p);

  string toString() const override;
  PatternPtr clone() const override;
  virtual void accept(ASTVisitor &visitor) const override { visitor.visit(this); }
};

struct StrPattern : public Pattern {
  string value;
  string prefix;

  StrPattern(string v, string p = "");
  StrPattern(const StrPattern &p);

  string toString() const override;
  PatternPtr clone() const override;
  virtual void accept(ASTVisitor &visitor) const override { visitor.visit(this); }
};

struct RangePattern : public Pattern {
  int start, end;

  RangePattern(int s, int e);
  RangePattern(const RangePattern &p);

  string toString() const override;
  PatternPtr clone() const override;
  virtual void accept(ASTVisitor &visitor) const override { visitor.visit(this); }
};

struct TuplePattern : public Pattern {
  vector<PatternPtr> patterns;

  TuplePattern(vector<PatternPtr> &&p);
  TuplePattern(const TuplePattern &p);

  string toString() const override;
  PatternPtr clone() const override;
  virtual void accept(ASTVisitor &visitor) const override { visitor.visit(this); }
};

struct ListPattern : public Pattern {
  vector<PatternPtr> patterns;

  ListPattern(vector<PatternPtr> &&p);
  ListPattern(const ListPattern &p);

  string toString() const override;
  PatternPtr clone() const override;
  virtual void accept(ASTVisitor &visitor) const override { visitor.visit(this); }
};

struct OrPattern : public Pattern {
  vector<PatternPtr> patterns;

  OrPattern(vector<PatternPtr> &&p);
  OrPattern(const OrPattern &p);

  string toString() const override;
  PatternPtr clone() const override;
  virtual void accept(ASTVisitor &visitor) const override { visitor.visit(this); }
};

struct WildcardPattern : public Pattern {
  string var;

  WildcardPattern(string v);
  WildcardPattern(const WildcardPattern &p);

  string toString() const override;
  PatternPtr clone() const override;
  virtual void accept(ASTVisitor &visitor) const override { visitor.visit(this); }
};

struct GuardedPattern : public Pattern {
  PatternPtr pattern;
  ExprPtr cond;

  GuardedPattern(PatternPtr p, ExprPtr c);
  GuardedPattern(const GuardedPattern &p);

  string toString() const override;
  PatternPtr clone() const override;
  virtual void accept(ASTVisitor &visitor) const override { visitor.visit(this); }
};

struct BoundPattern : public Pattern {
  string var;
  PatternPtr pattern;

  BoundPattern(string v, PatternPtr p);
  BoundPattern(const BoundPattern &p);

  string toString() const override;
  PatternPtr clone() const override;
  virtual void accept(ASTVisitor &visitor) const override { visitor.visit(this); }
};

} // namespace ast
} // namespace seq
