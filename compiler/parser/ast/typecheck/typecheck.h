/**
 * transform.h
 * Type checking AST walker.
 *
 * Simplifies a given AST and generates types for each expression node.
 */

#pragma once

#include <map>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "parser/ast/ast.h"
#include "parser/ast/format/format.h"
#include "parser/ast/typecheck/typecheck_ctx.h"
#include "parser/ast/types.h"
#include "parser/ast/visitor.h"
#include "parser/ast/walk.h"
#include "parser/common.h"

namespace seq {
namespace ast {

class TypecheckVisitor : public CallbackASTVisitor<ExprPtr, StmtPtr, PatternPtr> {
  std::shared_ptr<TypeContext> ctx;
  std::shared_ptr<std::vector<StmtPtr>> prependStmts;
  ExprPtr resultExpr;
  StmtPtr resultStmt;
  PatternPtr resultPattern;

  std::vector<types::Generic> parseGenerics(const std::vector<Param> &generics,
                                            int level);
  std::string patchIfRealizable(types::TypePtr typ, bool isClass);
  void fixExprName(ExprPtr &e, const std::string &newName);

  StmtPtr addMethod(Stmt *s, const std::string &canonicalName);
  types::FuncTypePtr
  findBestCall(types::ClassTypePtr c, const std::string &member,
               const std::vector<std::pair<std::string, types::TypePtr>> &args,
               bool failOnMultiple = false, types::TypePtr retType = nullptr);
  bool castToOptional(types::TypePtr lt, ExprPtr &rhs);
  bool getTupleIndex(types::ClassTypePtr tuple, const ExprPtr &expr,
                     const ExprPtr &index);
  ExprPtr visitDot(const ExprPtr &expr, const std::string &member,
                   std::vector<CallExpr::Arg> *args = nullptr);
  std::string generatePartialStub(const std::string &mask, const std::string &oldMask);
  std::vector<StmtPtr> parseClass(const ClassStmt *stmt);
  ExprPtr parseCall(const CallExpr *expr, types::TypePtr inType = nullptr,
                    ExprPtr *extraStage = nullptr);
  int reorder(const std::vector<std::pair<std::string, types::TypePtr>> &args,
              std::vector<std::pair<std::string, types::TypePtr>> &reorderedArgs,
              types::FuncTypePtr f);
  void addFunctionGenerics(types::FuncTypePtr t);

  void defaultVisit(const Expr *e) override;
  void defaultVisit(const Stmt *s) override;
  void defaultVisit(const Pattern *p) override;

public:
  static StmtPtr apply(std::shared_ptr<Cache> cache, StmtPtr stmts);
  TypecheckVisitor(std::shared_ptr<TypeContext> ctx,
                   std::shared_ptr<std::vector<StmtPtr>> stmts = nullptr);

  ExprPtr transform(const ExprPtr &e) override;
  StmtPtr transform(const StmtPtr &s) override;
  PatternPtr transform(const PatternPtr &p) override;
  ExprPtr transform(const ExprPtr &e, bool allowTypes);
  ExprPtr transformType(const ExprPtr &expr);

  types::TypePtr realizeFunc(types::TypePtr type);
  types::TypePtr realizeType(types::TypePtr type);
  StmtPtr realizeBlock(const StmtPtr &stmt, bool keepLast = false);

public:
  void visit(const BoolExpr *) override;
  void visit(const IntExpr *) override;
  void visit(const FloatExpr *) override;
  void visit(const StringExpr *) override;
  void visit(const IdExpr *) override;
  void visit(const IfExpr *) override;
  void visit(const BinaryExpr *) override;
  void visit(const PipeExpr *) override;
  void visit(const InstantiateExpr *) override;
  void visit(const SliceExpr *) override;
  void visit(const IndexExpr *) override;
  void visit(const CallExpr *) override;
  void visit(const StackAllocExpr *) override;
  void visit(const DotExpr *) override;
  void visit(const EllipsisExpr *) override;
  void visit(const TypeOfExpr *) override;
  void visit(const PtrExpr *) override;
  void visit(const YieldExpr *) override;
  void visit(const StmtExpr *) override;
  void visit(const StaticExpr *) override;

  void visit(const SuiteStmt *) override;
  void visit(const ExprStmt *) override;
  void visit(const AssignStmt *) override;
  void visit(const UpdateStmt *) override;
  void visit(const ReturnStmt *) override;
  void visit(const YieldStmt *) override;
  void visit(const AssertStmt *) override;
  void visit(const DelStmt *) override;
  void visit(const AssignMemberStmt *) override;
  void visit(const WhileStmt *) override;
  void visit(const ForStmt *) override;
  void visit(const IfStmt *) override;
  void visit(const MatchStmt *) override;
  void visit(const ExtendStmt *) override;
  void visit(const TryStmt *) override;
  void visit(const ThrowStmt *) override;
  void visit(const FunctionStmt *) override;
  void visit(const ClassStmt *) override;

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

  using CallbackASTVisitor<ExprPtr, StmtPtr, PatternPtr>::transform;

public:
  template <typename T> types::TypePtr forceUnify(const T *expr, types::TypePtr t) {
    if (expr->getType() && t) {
      types::Unification us;
      if (expr->getType()->unify(t, us) < 0) {
        us.undo();
        error(expr, "cannot unify {} and {}",
              expr->getType() ? expr->getType()->toString() : "-",
              t ? t->toString() : "-");
      }
    }
    return t;
  }
  template <typename T>
  types::TypePtr forceUnify(const std::unique_ptr<T> &expr, types::TypePtr t) {
    return forceUnify(expr.get(), t);
  }

  types::TypePtr forceUnify(types::TypePtr t, types::TypePtr u) {
    if (t && u) {
      types::Unification us;
      if (t->unify(u, us) >= 0)
        return t;
      us.undo();
    }
    error("cannot unify {} and {}", t ? t->toString() : "-", u ? u->toString() : "-");
    return nullptr;
  }
};

class StaticVisitor : public WalkVisitor {
  std::map<std::string, types::Generic> &generics;

public:
  bool evaluated;
  int value;

  using WalkVisitor::visit;
  StaticVisitor(std::map<std::string, types::Generic> &m);
  std::pair<bool, int> transform(const ExprPtr &e);
  void visit(const IdExpr *) override;
  void visit(const IntExpr *) override;
  void visit(const IfExpr *) override;
  void visit(const UnaryExpr *) override;
  void visit(const BinaryExpr *) override;
};

} // namespace ast
} // namespace seq
