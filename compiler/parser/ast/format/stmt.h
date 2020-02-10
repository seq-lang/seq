#pragma once

#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "parser/ast/expr.h"
#include "parser/ast/stmt.h"
#include "parser/ast/visitor.h"
#include "parser/common.h"
#include "parser/context.h"

namespace seq {
namespace ast {

class FormatStmtVisitor : public StmtVisitor {
  std::string result;
  int indent{0};

public:
  std::string transform(const StmtPtr &stmt, int indent = 0);
  std::string transform(const ExprPtr &stmt);
  std::string transform(const PatternPtr &stmt);

  std::string pad(int indent = 0);

  virtual void visit(const SuiteStmt *) override;
  virtual void visit(const PassStmt *) override;
  virtual void visit(const BreakStmt *) override;
  virtual void visit(const ContinueStmt *) override;
  virtual void visit(const ExprStmt *) override;
  virtual void visit(const AssignStmt *) override;
  virtual void visit(const DelStmt *) override;
  virtual void visit(const PrintStmt *) override;
  virtual void visit(const ReturnStmt *) override;
  virtual void visit(const YieldStmt *) override;
  virtual void visit(const AssertStmt *) override;
  virtual void visit(const TypeAliasStmt *) override;
  virtual void visit(const WhileStmt *) override;
  virtual void visit(const ForStmt *) override;
  virtual void visit(const IfStmt *) override;
  virtual void visit(const MatchStmt *) override;
  virtual void visit(const ExtendStmt *) override;
  virtual void visit(const ImportStmt *) override;
  virtual void visit(const ExternImportStmt *) override;
  virtual void visit(const TryStmt *) override;
  virtual void visit(const GlobalStmt *) override;
  virtual void visit(const ThrowStmt *) override;
  virtual void visit(const FunctionStmt *) override;
  virtual void visit(const ClassStmt *) override;
  virtual void visit(const DeclareStmt *) override;
  virtual void visit(const AssignEqStmt *) override;
  virtual void visit(const YieldFromStmt *) override;
  virtual void visit(const WithStmt *) override;
  virtual void visit(const PyDefStmt *) override;

  friend std::ostream &operator<<(std::ostream &out,
                                  const FormatStmtVisitor &c) {
    return out << c.result;
  }
};

} // namespace ast
} // namespace seq
