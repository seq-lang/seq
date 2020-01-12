#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "parser/expr.h"
#include "parser/pattern.h"
#include "seq/seq.h"

using std::ostream;
using std::string;
using std::unique_ptr;
using std::vector;
using std::pair;

struct Pattern : public seq::SrcObject {
  virtual ~Pattern() {}
  virtual string to_string() const = 0;
//   virtual void accept(PatternVisitor &) = 0;
  friend ostream &operator<<(ostream &out, const Pattern &c) {
    return out << c.to_string();
  }
};
typedef unique_ptr<Pattern> PatternPtr;

struct StarPattern : public Pattern {
    StarPattern();
    string to_string() const;
    // ACCEPT_VISITOR(Pattern);
};

struct IntPattern : public Pattern {
    int value;
    IntPattern(int v);
    string to_string() const;
    // ACCEPT_VISITOR(Pattern);
};

struct BoolPattern : public Pattern {
    bool value;
    BoolPattern(bool v);
    string to_string() const;
    // ACCEPT_VISITOR(Pattern);
};

struct StrPattern : public Pattern {
    string value;
    StrPattern(string v);
    string to_string() const;
    // ACCEPT_VISITOR(Pattern);
};

struct SeqPattern : public Pattern {
    string value;
    SeqPattern(string v);
    string to_string() const;
    // ACCEPT_VISITOR(Pattern);
};

struct RangePattern : public Pattern {
    int start, end;
    RangePattern(int s, int e);
    string to_string() const;
    // ACCEPT_VISITOR(Pattern);
};

struct TuplePattern : public Pattern {
    vector<PatternPtr> patterns;
    TuplePattern(vector<PatternPtr> p);
    string to_string() const;
    // ACCEPT_VISITOR(Pattern);
};

struct ListPattern : public Pattern {
    vector<PatternPtr> patterns;
    ListPattern(vector<PatternPtr> p);
    string to_string() const;
    // ACCEPT_VISITOR(Pattern);
};

struct OrPattern : public Pattern {
    vector<PatternPtr> patterns;
    OrPattern(vector<PatternPtr> p);
    string to_string() const;
    // ACCEPT_VISITOR(Pattern);
};

struct WildcardPattern : public Pattern {
    string var;
    WildcardPattern(string v);
    string to_string() const;
    // ACCEPT_VISITOR(Pattern);
};

struct GuardedPattern : public Pattern {
    PatternPtr pattern;
    ExprPtr cond;
    GuardedPattern(PatternPtr p, ExprPtr c);
    string to_string() const;
    // ACCEPT_VISITOR(Pattern);
};

struct BoundPattern : public Pattern {
    string var;
    PatternPtr pattern;
    BoundPattern(string v, PatternPtr p);
    string to_string() const;
    // ACCEPT_VISITOR(Pattern);
};
