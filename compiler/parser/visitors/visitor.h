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
  virtual void defaultVisit(Expr *expr);
  /// Default statement node visitor if a particular visitor is not overloaded.
  virtual void defaultVisit(Stmt *stmt);
  /// Default pattern node visitor if a particular visitor is not overloaded.
  virtual void defaultVisit(Pattern *pattern);

public:
  virtual void visit(NoneExpr *);
  virtual void visit(BoolExpr *);
  virtual void visit(IntExpr *);
  virtual void visit(FloatExpr *);
  virtual void visit(StringExpr *);
  virtual void visit(IdExpr *);
  virtual void visit(StarExpr *);
  virtual void visit(TupleExpr *);
  virtual void visit(ListExpr *);
  virtual void visit(SetExpr *);
  virtual void visit(DictExpr *);
  virtual void visit(GeneratorExpr *);
  virtual void visit(DictGeneratorExpr *);
  virtual void visit(IfExpr *);
  virtual void visit(UnaryExpr *);
  virtual void visit(BinaryExpr *);
  virtual void visit(PipeExpr *);
  virtual void visit(IndexExpr *);
  virtual void visit(CallExpr *);
  virtual void visit(DotExpr *);
  virtual void visit(SliceExpr *);
  virtual void visit(EllipsisExpr *);
  virtual void visit(TypeOfExpr *);
  virtual void visit(LambdaExpr *);
  virtual void visit(YieldExpr *);
  virtual void visit(AssignExpr *);
  virtual void visit(PtrExpr *);
  virtual void visit(TupleIndexExpr *);
  virtual void visit(StackAllocExpr *);
  virtual void visit(InstantiateExpr *);
  virtual void visit(StaticExpr *);
  virtual void visit(StmtExpr *);

  virtual void visit(AssignMemberStmt *);
  virtual void visit(UpdateStmt *);
  virtual void visit(SuiteStmt *);
  virtual void visit(PassStmt *);
  virtual void visit(BreakStmt *);
  virtual void visit(ContinueStmt *);
  virtual void visit(ExprStmt *);
  virtual void visit(AssignStmt *);
  virtual void visit(DelStmt *);
  virtual void visit(PrintStmt *);
  virtual void visit(ReturnStmt *);
  virtual void visit(YieldStmt *);
  virtual void visit(AssertStmt *);
  virtual void visit(WhileStmt *);
  virtual void visit(ForStmt *);
  virtual void visit(IfStmt *);
  virtual void visit(MatchStmt *);
  virtual void visit(ImportStmt *);
  virtual void visit(TryStmt *);
  virtual void visit(GlobalStmt *);
  virtual void visit(ThrowStmt *);
  virtual void visit(FunctionStmt *);
  virtual void visit(ClassStmt *);
  virtual void visit(YieldFromStmt *);
  virtual void visit(WithStmt *);

  virtual void visit(StarPattern *);
  virtual void visit(IntPattern *);
  virtual void visit(BoolPattern *);
  virtual void visit(StrPattern *);
  virtual void visit(RangePattern *);
  virtual void visit(TuplePattern *);
  virtual void visit(ListPattern *);
  virtual void visit(OrPattern *);
  virtual void visit(WildcardPattern *);
  virtual void visit(GuardedPattern *);
  virtual void visit(BoundPattern *);
};

template <typename TE, typename TS, typename TP>
/**
 * Callback AST visitor.
 * This visitor extends base ASTVisitor and stores node's source location (SrcObject).
 * Function simplify() will visit a node and return the appropriate transformation. As
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
  template <typename Tn, typename... Ts> auto N(Ts &&...args) {
    auto t = std::make_unique<Tn>(std::forward<Ts>(args)...);
    t->setSrcInfo(getSrcInfo());
    return t;
  }

  /// Convenience method that constructs a node.
  /// @param s source location.
  template <typename Tn, typename... Ts>
  auto Nx(const seq::SrcObject *s, Ts &&...args) {
    auto t = std::make_unique<Tn>(std::forward<Ts>(args)...);
    t->setSrcInfo(s->getSrcInfo());
    return t;
  }

  /// Convenience method that raises an error at the current source location.
  template <typename... TArgs> void error(const char *format, TArgs &&...args) {
    ast::error(getSrcInfo(), fmt::format(format, args...).c_str());
  }

  /// Convenience method that raises an error at the source location of p.
  template <typename T, typename... TArgs>
  void error(const T &p, const char *format, TArgs &&...args) {
    ast::error(p->getSrcInfo(), fmt::format(format, args...).c_str());
  }

  /// Convenience method that raises an internal error.
  template <typename T, typename... TArgs>
  void internalError(const char *format, TArgs &&...args) {
    throw exc::ParserException(
        fmt::format("INTERNAL: {}", fmt::format(format, args...), getSrcInfo()));
  }
};

} // namespace ast
} // namespace seq
