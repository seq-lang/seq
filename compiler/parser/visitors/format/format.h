/**
 * format.h
 * Format AST walker.
 *
 * Generates HTML representation of a given AST node.
 * Useful for debugging types.
 */

#pragma once

#include <ostream>
#include <string>
#include <vector>

#include "parser/ast.h"
#include "parser/cache.h"
#include "parser/common.h"
#include "parser/visitors/visitor.h"

namespace seq {
namespace ast {

class FormatVisitor : public CallbackASTVisitor<string, string, string> {
  string result;
  string space;
  bool renderType, renderHTML;
  int indent;

  string header, footer, nl;
  string typeStart, typeEnd;
  string nodeStart, nodeEnd;
  string exprStart, exprEnd;
  string commentStart, commentEnd;
  string keywordStart, keywordEnd;

  shared_ptr<Cache> cache;

private:
  template <typename T, typename... Ts> string renderExpr(T &&t, Ts &&... args) {
    string s;
    if (renderType)
      s += fmt::format("{}{}{}", typeStart,
                       t->getType() ? t->getType()->toString() : "-", typeEnd);
    return fmt::format("{}{}{}{}{}{}", exprStart, s, nodeStart, fmt::format(args...),
                       nodeEnd, exprEnd);
  }
  template <typename... Ts> string renderComment(Ts &&... args) {
    return fmt::format("{}{}{}", commentStart, fmt::format(args...), commentEnd);
  }
  string pad(int indent = 0) const;
  string newline() const;
  string keyword(const string &s) const;

public:
  FormatVisitor(bool html, shared_ptr<Cache> cache = nullptr);
  string transform(const ExprPtr &e) override;
  string transform(const StmtPtr &stmt) override;
  string transform(const PatternPtr &ptr) override;
  string transform(const StmtPtr &stmt, int indent);

  template <typename T>
  static string apply(const T &stmt, shared_ptr<Cache> cache = nullptr,
                      bool html = false, bool init = false) {
    auto t = FormatVisitor(html, cache);
    return fmt::format("{}{}{}", t.header, t.transform(stmt), t.footer);
  }

  void defaultVisit(const Expr *e) override { error("cannot format {}", *e); }
  void defaultVisit(const Stmt *e) override { error("cannot format {}", *e); }
  void defaultVisit(const Pattern *e) override { error("cannot format {}", *e); }

public:
  void visit(const NoneExpr *) override;
  void visit(const BoolExpr *) override;
  void visit(const IntExpr *) override;
  void visit(const FloatExpr *) override;
  void visit(const StringExpr *) override;
  void visit(const IdExpr *) override;
  void visit(const StarExpr *) override;
  void visit(const TupleExpr *) override;
  void visit(const ListExpr *) override;
  void visit(const SetExpr *) override;
  void visit(const DictExpr *) override;
  void visit(const GeneratorExpr *) override;
  void visit(const DictGeneratorExpr *) override;
  void visit(const InstantiateExpr *expr) override;
  void visit(const StackAllocExpr *expr) override;
  void visit(const IfExpr *) override;
  void visit(const UnaryExpr *) override;
  void visit(const BinaryExpr *) override;
  void visit(const PipeExpr *) override;
  void visit(const IndexExpr *) override;
  void visit(const CallExpr *) override;
  void visit(const DotExpr *) override;
  void visit(const SliceExpr *) override;
  void visit(const EllipsisExpr *) override;
  void visit(const TypeOfExpr *) override;
  void visit(const PtrExpr *) override;
  void visit(const LambdaExpr *) override;
  void visit(const YieldExpr *) override;
  void visit(const StaticExpr *) override;
  void visit(const StmtExpr *expr) override;

  void visit(const SuiteStmt *) override;
  void visit(const PassStmt *) override;
  void visit(const BreakStmt *) override;
  void visit(const UpdateStmt *) override;
  void visit(const ContinueStmt *) override;
  void visit(const ExprStmt *) override;
  void visit(const AssignStmt *) override;
  void visit(const AssignMemberStmt *) override;
  void visit(const DelStmt *) override;
  void visit(const PrintStmt *) override;
  void visit(const ReturnStmt *) override;
  void visit(const YieldStmt *) override;
  void visit(const AssertStmt *) override;
  void visit(const WhileStmt *) override;
  void visit(const ForStmt *) override;
  void visit(const IfStmt *) override;
  void visit(const MatchStmt *) override;
  void visit(const ImportStmt *) override;
  void visit(const TryStmt *) override;
  void visit(const GlobalStmt *) override;
  void visit(const ThrowStmt *) override;
  void visit(const FunctionStmt *) override;
  void visit(const ClassStmt *) override;
  void visit(const YieldFromStmt *) override;
  void visit(const WithStmt *) override;

  void visit(const StarPattern *) override;
  void visit(const IntPattern *) override;
  void visit(const BoolPattern *) override;
  void visit(const StrPattern *) override;
  void visit(const RangePattern *) override;
  void visit(const TuplePattern *) override;
  void visit(const ListPattern *) override;
  void visit(const OrPattern *) override;
  void visit(const WildcardPattern *) override;
  void visit(const GuardedPattern *) override;
  void visit(const BoundPattern *) override;

public:
  friend std::ostream &operator<<(std::ostream &out, const FormatVisitor &c) {
    return out << c.result;
  }

  using CallbackASTVisitor<string, string, string>::transform;
  template <typename T> string transform(const vector<T> &ts) {
    vector<string> r;
    for (auto &e : ts)
      r.push_back(transform(e));
    return fmt::format("{}", fmt::join(r, ", "));
  }
};

} // namespace ast
} // namespace seq
