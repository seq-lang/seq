#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "parser/ast/pattern.h"
#include "parser/ast/transform/stmt.h"

using std::string;

class FormatPatternVisitor : public PatternVisitor {
  string result;

public:
  string transform(const Pattern *ptr);

  template <typename T>
  auto transform(const unique_ptr<T> &t) -> decltype(transform(t.get())) {
    return transform(t.get());
  }

  void visit(const StarPattern *) override;
  void visit(const IntPattern *) override;
  void visit(const BoolPattern *) override;
  void visit(const StrPattern *) override;
  void visit(const SeqPattern *) override;
  void visit(const RangePattern *) override;
  void visit(const TuplePattern *) override;
  void visit(const ListPattern *) override;
  void visit(const OrPattern *) override;
  void visit(const WildcardPattern *) override;
  void visit(const GuardedPattern *) override;
  void visit(const BoundPattern *) override;
};
