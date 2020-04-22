/**
 * doc.h
 * Documentation AST walker.
 *
 * Reads docstrings and generates documentation of a given AST node.
 */

#pragma once

#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "parser/ast/ast.h"
#include "parser/ast/visitor.h"
#include "parser/common.h"
#include "parser/context.h"

namespace seq {
namespace ast {
#ifdef HAHA2

class DocStmtVisitor : public StmtVisitor {
public:
  virtual void visit(const SuiteStmt *) override;
  virtual void visit(const FunctionStmt *) override;
  virtual void visit(const ClassStmt *) override;
};

#endif

} // namespace ast
} // namespace seq
