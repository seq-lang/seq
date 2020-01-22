#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "parser/ast/expr.h"
#include "parser/ast/pattern.h"
#include "parser/ast/visitor.h"
#include "seq/seq.h"

using std::ostream;
using std::pair;
using std::string;
using std::unique_ptr;
using std::vector;

#define ACCEPT_VISITOR                                                         \
  virtual void accept(PatternVisitor &visitor) const override {                \
    visitor.visit(this);                                                       \
  }

struct Pattern : public seq::SrcObject {
  virtual ~Pattern() {}
  virtual string to_string() const = 0;
  virtual void accept(PatternVisitor &) const = 0;
  friend ostream &operator<<(ostream &out, const Pattern &c) {
    return out << c.to_string();
  }
};
typedef unique_ptr<Pattern> PatternPtr;

struct StarPattern : public Pattern {
  StarPattern();
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct IntPattern : public Pattern {
  int value;
  IntPattern(int v);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct BoolPattern : public Pattern {
  bool value;
  BoolPattern(bool v);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct StrPattern : public Pattern {
  string value;
  StrPattern(string v);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct SeqPattern : public Pattern {
  string value;
  SeqPattern(string v);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct RangePattern : public Pattern {
  int start, end;
  RangePattern(int s, int e);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct TuplePattern : public Pattern {
  vector<PatternPtr> patterns;
  TuplePattern(vector<PatternPtr> p);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct ListPattern : public Pattern {
  vector<PatternPtr> patterns;
  ListPattern(vector<PatternPtr> p);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct OrPattern : public Pattern {
  vector<PatternPtr> patterns;
  OrPattern(vector<PatternPtr> p);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct WildcardPattern : public Pattern {
  string var;
  WildcardPattern(string v);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct GuardedPattern : public Pattern {
  PatternPtr pattern;
  ExprPtr cond;
  GuardedPattern(PatternPtr p, ExprPtr c);
  string to_string() const override;
  ACCEPT_VISITOR;
};

struct BoundPattern : public Pattern {
  string var;
  PatternPtr pattern;
  BoundPattern(string v, PatternPtr p);
  string to_string() const override;
  ACCEPT_VISITOR;
};

#undef ACCEPT_VISITOR
