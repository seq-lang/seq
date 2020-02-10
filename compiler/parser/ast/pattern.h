#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/expr.h"
#include "parser/ast/pattern.h"
#include "parser/ast/visitor.h"

#define ACCEPT_VISITOR                                                         \
  virtual void accept(PatternVisitor &visitor) const override {                \
    visitor.visit(this);                                                       \
  }

namespace seq {
namespace ast {

struct Pattern : public seq::SrcObject {
  virtual ~Pattern() {}
  virtual std::string to_string() const = 0;
  virtual void accept(PatternVisitor &) const = 0;
  friend std::ostream &operator<<(std::ostream &out, const Pattern &c) {
    return out << c.to_string();
  }
};
typedef std::unique_ptr<Pattern> PatternPtr;

struct StarPattern : public Pattern {
  StarPattern();
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct IntPattern : public Pattern {
  int value;
  IntPattern(int v);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct BoolPattern : public Pattern {
  bool value;
  BoolPattern(bool v);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct StrPattern : public Pattern {
  std::string value;
  StrPattern(std::string v);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct SeqPattern : public Pattern {
  std::string value;
  SeqPattern(std::string v);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct RangePattern : public Pattern {
  int start, end;
  RangePattern(int s, int e);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct TuplePattern : public Pattern {
  std::vector<PatternPtr> patterns;
  TuplePattern(std::vector<PatternPtr> p);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct ListPattern : public Pattern {
  std::vector<PatternPtr> patterns;
  ListPattern(std::vector<PatternPtr> p);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct OrPattern : public Pattern {
  std::vector<PatternPtr> patterns;
  OrPattern(std::vector<PatternPtr> p);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct WildcardPattern : public Pattern {
  std::string var;
  WildcardPattern(std::string v);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct GuardedPattern : public Pattern {
  PatternPtr pattern;
  ExprPtr cond;
  GuardedPattern(PatternPtr p, ExprPtr c);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

struct BoundPattern : public Pattern {
  std::string var;
  PatternPtr pattern;
  BoundPattern(std::string v, PatternPtr p);
  std::string to_string() const override;
  ACCEPT_VISITOR;
};

} // namespace ast
} // namespace seq

#undef ACCEPT_VISITOR
