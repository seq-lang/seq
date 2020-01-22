#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "parser/ast/pattern.h"
#include "parser/ast/transform/stmt.h"


using std::ostream;
using std::pair;
using std::string;
using std::unique_ptr;
using std::vector;

class TransformPatternVisitor : public PatternVisitor {
  PatternPtr result;
  friend TransformStmtVisitor;

public:
  TransformPatternVisitor();
  PatternPtr transform(const Pattern *ptr);
  vector<PatternPtr> transform(const vector<PatternPtr> &pats);

  template <typename T>
  auto transform(const unique_ptr<T> &t) -> decltype(transform(t.get())) {
    return transform(t.get());
  }

  void visit(const TuplePattern *) override;
  void visit(const ListPattern *) override;
  void visit(const OrPattern *) override;
  void visit(const GuardedPattern *) override;
  void visit(const BoundPattern *) override;
};
