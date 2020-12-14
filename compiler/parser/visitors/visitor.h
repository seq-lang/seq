/*
 * visitor.h --- Seq AST visitors.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#pragma once

#include <memory>
#include <vector>

#include "parser/ast.h"
#include "parser/common.h"

namespace seq {
namespace ast {

/**
 * Base Seq AST visitor.
 * Each visit() by default calls an appropriate defaultVisit().
 */
struct ASTVisitor {
protected:
  /// Default expression node visitor if a particular visitor is not overloaded.
  virtual void defaultVisit(const Expr *expr);
  /// Default statement node visitor if a particular visitor is not overloaded.
  virtual void defaultVisit(const Stmt *stmt);
  /// Default pattern node visitor if a particular visitor is not overloaded.
  virtual void defaultVisit(const Pattern *pattern);

public:
  virtual void visit(const NoneExpr *);
  virtual void visit(const BoolExpr *);
  virtual void visit(const IntExpr *);
  virtual void visit(const FloatExpr *);
  virtual void visit(const StringExpr *);
  virtual void visit(const IdExpr *);
  virtual void visit(const StarExpr *);
  virtual void visit(const TupleExpr *);
  virtual void visit(const ListExpr *);
  virtual void visit(const SetExpr *);
  virtual void visit(const DictExpr *);
  virtual void visit(const GeneratorExpr *);
  virtual void visit(const DictGeneratorExpr *);
  virtual void visit(const IfExpr *);
  virtual void visit(const UnaryExpr *);
  virtual void visit(const BinaryExpr *);
  virtual void visit(const PipeExpr *);
  virtual void visit(const IndexExpr *);
  virtual void visit(const CallExpr *);
  virtual void visit(const DotExpr *);
  virtual void visit(const SliceExpr *);
  virtual void visit(const EllipsisExpr *);
  virtual void visit(const TypeOfExpr *);
  virtual void visit(const LambdaExpr *);
  virtual void visit(const YieldExpr *);
  virtual void visit(const PtrExpr *);
  virtual void visit(const TupleIndexExpr *);
  virtual void visit(const StackAllocExpr *);
  virtual void visit(const InstantiateExpr *);
  virtual void visit(const StaticExpr *);
  virtual void visit(const StmtExpr *);

  virtual void visit(const AssignMemberStmt *);
  virtual void visit(const UpdateStmt *);
  virtual void visit(const SuiteStmt *);
  virtual void visit(const PassStmt *);
  virtual void visit(const BreakStmt *);
  virtual void visit(const ContinueStmt *);
  virtual void visit(const ExprStmt *);
  virtual void visit(const AssignStmt *);
  virtual void visit(const DelStmt *);
  virtual void visit(const PrintStmt *);
  virtual void visit(const ReturnStmt *);
  virtual void visit(const YieldStmt *);
  virtual void visit(const AssertStmt *);
  virtual void visit(const WhileStmt *);
  virtual void visit(const ForStmt *);
  virtual void visit(const IfStmt *);
  virtual void visit(const MatchStmt *);
  virtual void visit(const ImportStmt *);
  virtual void visit(const TryStmt *);
  virtual void visit(const GlobalStmt *);
  virtual void visit(const ThrowStmt *);
  virtual void visit(const FunctionStmt *);
  virtual void visit(const ClassStmt *);
  virtual void visit(const YieldFromStmt *);
  virtual void visit(const WithStmt *);

  virtual void visit(const StarPattern *);
  virtual void visit(const IntPattern *);
  virtual void visit(const BoolPattern *);
  virtual void visit(const StrPattern *);
  virtual void visit(const RangePattern *);
  virtual void visit(const TuplePattern *);
  virtual void visit(const ListPattern *);
  virtual void visit(const OrPattern *);
  virtual void visit(const WildcardPattern *);
  virtual void visit(const GuardedPattern *);
  virtual void visit(const BoundPattern *);
};

template <typename TE, typename TS, typename TP>
/**
 * Callback AST visitor.
 * This visitor extends base ASTVisitor and stores node's source location (SrcObject).
 * Function transform() will visit a node and return the appropriate transformation. As
 * each node type (expression, statement, or a pattern) might return a different type,
 * this visitor is generic for each different return type.
 */
struct CallbackASTVisitor : public ASTVisitor, public SrcObject {
  virtual TE transform(const unique_ptr<Expr> &expr) = 0;
  virtual TS transform(const unique_ptr<Stmt> &stmt) = 0;
  virtual TP transform(const unique_ptr<Pattern> &pattern) = 0;

  /// Convenience method that transforms a vector of nodes.
  template <typename T> auto transform(const vector<T> &ts) {
    vector<T> r;
    for (auto &e : ts)
      r.push_back(transform(e));
    return r;
  }

  /// Convenience method that constructs a node with the visitor's source location.
  template <typename Tn, typename... Ts> auto N(Ts &&... args) {
    auto t = std::make_unique<Tn>(std::forward<Ts>(args)...);
    t->setSrcInfo(getSrcInfo());
    return t;
  }

  /// Convenience method that constructs a node.
  /// @param s source location.
  template <typename Tn, typename... Ts>
  auto Nx(const seq::SrcObject *s, Ts &&... args) {
    auto t = std::make_unique<Tn>(std::forward<Ts>(args)...);
    t->setSrcInfo(s->getSrcInfo());
    return t;
  }

  /// Convenience method that raises an error at the current source location.
  template <typename... TArgs> void error(const char *format, TArgs &&... args) {
    ast::error(getSrcInfo(), fmt::format(format, args...).c_str());
  }

  /// Convenience method that raises an error at the source location of p.
  template <typename T, typename... TArgs>
  void error(const T &p, const char *format, TArgs &&... args) {
    ast::error(p->getSrcInfo(), fmt::format(format, args...).c_str());
  }

  /// Convenience method that raises an internal error.
  template <typename T, typename... TArgs>
  void internalError(const char *format, TArgs &&... args) {
    throw exc::ParserException(
        fmt::format("INTERNAL: {}", fmt::format(format, args...), getSrcInfo()));
  }
};

} // namespace ast
} // namespace seq
