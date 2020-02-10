#pragma once

#include <memory>
#include <ostream>
#include <string>

#include "parser/ast/pattern.h"
#include "parser/ast/transform/stmt.h"

namespace seq {
namespace ast {

class FormatPatternVisitor : public PatternVisitor {
  std::string result;

public:
  std::string transform(const Pattern *ptr);

  template <typename T>
  auto transform(const std::unique_ptr<T> &t) -> decltype(transform(t.get())) {
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

} // namespace ast
} // namespace seq
