#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "parser/ast/pattern.h"
#include "parser/ast/codegen/stmt.h"

class CodegenPatternVisitor : public PatternVisitor {
  CodegenStmtVisitor &stmtVisitor;
  seq::Pattern *result;
  friend class CodegenStmtVisitor;

public:
  CodegenPatternVisitor(CodegenStmtVisitor &);
  seq::Pattern *transform(const PatternPtr &ptr);

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
