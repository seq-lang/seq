/**
 * transform.h
 * Type checking AST walker.
 *
 * Simplifies a given AST and generates types for each expression node.
 */

#pragma once

#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "parser/ast/ast.h"
#include "parser/ast/format.h"
#include "parser/ast/transform_ctx.h"
#include "parser/ast/types.h"
#include "parser/ast/visitor.h"
#include "parser/ast/walk.h"
#include "parser/common.h"

namespace seq {
namespace ast {

class CaptureVisitor : public WalkVisitor {
  std::shared_ptr<TypeContext> ctx;

public:
  std::unordered_set<std::string> captures;
  using WalkVisitor::visit;
  CaptureVisitor(std::shared_ptr<TypeContext> ctx);
  void visit(const IdExpr *) override;
};

class StaticVisitor : public WalkVisitor {
  std::shared_ptr<TypeContext> ctx;
  const std::unordered_map<std::string, types::Generic> *map;

public:
  std::map<std::string, types::Generic> captures; // map so it is sorted
  bool evaluated;
  int value;

  using WalkVisitor::visit;
  StaticVisitor(std::shared_ptr<TypeContext> ctx,
                const std::unordered_map<std::string, types::Generic> *m = nullptr);
  std::pair<bool, int> transform(const Expr *e);
  void visit(const IdExpr *) override;
  void visit(const IntExpr *) override;
  void visit(const IfExpr *) override;
  void visit(const UnaryExpr *) override;
  void visit(const BinaryExpr *) override;
};

class TransformVisitor : public ASTVisitor, public SrcObject {
  std::shared_ptr<TypeContext> ctx;
  std::shared_ptr<std::vector<StmtPtr>> prependStmts;
  ExprPtr resultExpr;
  StmtPtr resultStmt;
  PatternPtr resultPattern;

  /// Helper function that handles simple assignments
  /// (e.g. a = b, a.x = b or a[x] = b)
  StmtPtr addAssignment(const Expr *lhs, const Expr *rhs, const Expr *type = nullptr,
                        bool force = false);
  /// Helper function that decomposes complex assignments into simple ones
  /// (e.g. a, *b, (c, d) = foo)
  void processAssignment(const Expr *lhs, const Expr *rhs, std::vector<StmtPtr> &stmts,
                         bool force = false);

  StmtPtr getGeneratorBlock(const std::vector<GeneratorExpr::Body> &loops,
                            SuiteStmt *&prev);
  void prepend(StmtPtr s);

  std::string patchIfRealizable(types::TypePtr typ, bool isClass);
  void fixExprName(ExprPtr &e, const std::string &newName);

  std::shared_ptr<TypeItem::Item> processIdentifier(std::shared_ptr<TypeContext> tctx,
                                                    const std::string &id);

  RealizationContext::FuncRealization realizeFunc(types::FuncTypePtr type);
  RealizationContext::ClassRealization realizeType(types::ClassTypePtr type);
  int realizeStatic(types::StaticTypePtr st);

  ExprPtr conditionalMagic(const ExprPtr &expr, const std::string &type,
                           const std::string &magic);
  ExprPtr makeBoolExpr(const ExprPtr &e);
  std::vector<types::Generic> parseGenerics(const std::vector<Param> &generics);

  StmtPtr addMethod(Stmt *s, const std::string &canonicalName);
  types::FuncTypePtr
  findBestCall(types::ClassTypePtr c, const std::string &member,
               const std::vector<std::pair<std::string, types::TypePtr>> &args,
               bool failOnMultiple = false, types::TypePtr retType = nullptr);

  bool wrapOptional(types::TypePtr lt, ExprPtr &rhs);
  std::string generateFunctionStub(int len);
  std::string generateTupleStub(int len);
  std::string generatePartialStub(const std::string &flag);

  // std::vector<int> callCallable(types::ClassTypePtr f,
  // std::vector<CallExpr::Arg> &args, std::vector<CallExpr::Arg>
  // &reorderedArgs);
  std::vector<int> callFunc(types::ClassTypePtr f, std::vector<CallExpr::Arg> &args,
                            std::vector<CallExpr::Arg> &reorderedArgs,
                            const std::vector<int> &availableArguments);
  // std::vector<int> callPartial(types::PartialTypePtr f,
  //  std::vector<CallExpr::Arg> &args,
  //  std::vector<CallExpr::Arg> &reorderedArgs);
  bool handleStackAlloc(const CallExpr *expr);
  bool getTupleIndex(types::ClassTypePtr tuple, const ExprPtr &expr,
                     const ExprPtr &index);
  StmtPtr makeInternalFn(const std::string &name, ExprPtr &&ret, Param &&arg = Param(),
                         Param &&arg2 = Param());
  StmtPtr makeInternalFn(const std::string &name, ExprPtr &&ret,
                         std::vector<Param> &&args);

public:
  TransformVisitor(std::shared_ptr<TypeContext> ctx,
                   std::shared_ptr<std::vector<StmtPtr>> stmts = nullptr);

  ExprPtr transform(const Expr *e, bool allowTypes = false);
  StmtPtr transform(const Stmt *s);
  PatternPtr transform(const Pattern *p);
  ExprPtr transformType(const ExprPtr &expr);
  StmtPtr realizeBlock(const Stmt *stmt, bool keepLast = false);

public:
  void visit(const NoneExpr *) override;
  void visit(const BoolExpr *) override;
  void visit(const IntExpr *) override;
  void visit(const FloatExpr *) override;
  void visit(const StringExpr *) override;
  void visit(const FStringExpr *) override;
  void visit(const KmerExpr *) override;
  void visit(const SeqExpr *) override;
  void visit(const IdExpr *) override;
  void visit(const UnpackExpr *) override;
  void visit(const TupleExpr *) override;
  void visit(const ListExpr *) override;
  void visit(const SetExpr *) override;
  void visit(const DictExpr *) override;
  void visit(const GeneratorExpr *) override;
  void visit(const DictGeneratorExpr *) override;
  void visit(const IfExpr *) override;
  void visit(const UnaryExpr *) override;
  void visit(const BinaryExpr *) override;
  void visit(const PipeExpr *) override;
  void visit(const IndexExpr *) override;
  void visit(const TupleIndexExpr *) override;
  void visit(const CallExpr *) override;
  void visit(const StackAllocExpr *) override;
  void visit(const DotExpr *) override;
  void visit(const SliceExpr *) override;
  void visit(const EllipsisExpr *) override;
  void visit(const TypeOfExpr *) override;
  void visit(const PtrExpr *) override;
  void visit(const LambdaExpr *) override;
  void visit(const YieldExpr *) override;

  void visit(const SuiteStmt *) override;
  void visit(const PassStmt *) override;
  void visit(const BreakStmt *) override;
  void visit(const ContinueStmt *) override;
  void visit(const ExprStmt *) override;
  void visit(const AssignStmt *) override;
  void visit(const UpdateStmt *) override;
  void visit(const DelStmt *) override;
  void visit(const PrintStmt *) override;
  void visit(const ReturnStmt *) override;
  void visit(const YieldStmt *) override;
  void visit(const AssertStmt *) override;
  void visit(const AssignMemberStmt *) override;
  void visit(const WhileStmt *) override;
  void visit(const ForStmt *) override;
  void visit(const IfStmt *) override;
  void visit(const MatchStmt *) override;
  void visit(const ExtendStmt *) override;
  void visit(const ImportStmt *) override;
  void visit(const ExternImportStmt *) override;
  void visit(const TryStmt *) override;
  void visit(const GlobalStmt *) override;
  void visit(const ThrowStmt *) override;
  void visit(const FunctionStmt *) override;
  void visit(const ClassStmt *) override;
  void visit(const AssignEqStmt *) override;
  void visit(const YieldFromStmt *) override;
  void visit(const WithStmt *) override;
  void visit(const PyDefStmt *) override;

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

public:
  template <typename Tn, typename... Ts> auto N(Ts &&... args) {
    auto t = std::make_unique<Tn>(std::forward<Ts>(args)...);
    t->setSrcInfo(getSrcInfo());
    return t;
  }
  template <typename Tn, typename... Ts>
  auto Nx(const seq::SrcObject *s, Ts &&... args) {
    auto t = std::make_unique<Tn>(std::forward<Ts>(args)...);
    t->setSrcInfo(s->getSrcInfo());
    return t;
  }
  template <typename Tt, typename... Ts> auto T(Ts &&... args) {
    auto t = std::make_shared<Tt>(std::forward<Ts>(args)...);
    t->setSrcInfo(getSrcInfo());
    return t;
  }
  template <typename T, typename... Ts>
  auto transform(const std::unique_ptr<T> &t, Ts &&... args)
      -> decltype(transform(t.get())) {
    return transform(t.get(), std::forward<Ts>(args)...);
  }
  template <typename T> auto transform(const std::vector<T> &ts) {
    std::vector<T> r;
    for (auto &e : ts)
      r.push_back(transform(e));
    return r;
  }

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

  template <typename... TArgs> void error(const char *format, TArgs &&... args) {
    ast::error(getSrcInfo(), fmt::format(format, args...).c_str());
  }

  template <typename T, typename... TArgs>
  void error(const T &p, const char *format, TArgs &&... args) {
    ast::error(p->getSrcInfo(), fmt::format(format, args...).c_str());
  }

  template <typename T, typename... TArgs>
  void internalError(const char *format, TArgs &&... args) {
    throw exc::ParserException(
        fmt::format("INTERNAL: {}", fmt::format(format, args...), getSrcInfo()));
  }
};

} // namespace ast
} // namespace seq
