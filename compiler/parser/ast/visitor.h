#pragma once

#include <memory>
#include <vector>

#include "parser/common.h"

namespace seq {
namespace ast {

struct Expr;
struct Stmt;
struct Pattern;

struct NoneExpr;
struct BoolExpr;
struct IntExpr;
struct FloatExpr;
struct StringExpr;
struct FStringExpr;
struct KmerExpr;
struct SeqExpr;
struct IdExpr;
struct UnpackExpr;
struct TupleExpr;
struct ListExpr;
struct SetExpr;
struct DictExpr;
struct GeneratorExpr;
struct DictGeneratorExpr;
struct IfExpr;
struct UnaryExpr;
struct BinaryExpr;
struct PipeExpr;
struct IndexExpr;
struct CallExpr;
struct DotExpr;
struct SliceExpr;
struct EllipsisExpr;
struct TypeOfExpr;
struct PtrExpr;
struct LambdaExpr;
struct YieldExpr;
struct TupleIndexExpr;
struct StackAllocExpr;
struct StaticExpr;
struct InstantiateExpr;
struct StmtExpr;

struct AssignMemberStmt;
struct SuiteStmt;
struct PassStmt;
struct BreakStmt;
struct ContinueStmt;
struct ExprStmt;
struct AssignStmt;
struct DelStmt;
struct PrintStmt;
struct ReturnStmt;
struct YieldStmt;
struct AssertStmt;
struct WhileStmt;
struct ForStmt;
struct IfStmt;
struct MatchStmt;
struct ExtendStmt;
struct ImportStmt;
struct ExternImportStmt;
struct TryStmt;
struct GlobalStmt;
struct ThrowStmt;
struct FunctionStmt;
struct ClassStmt;
struct AssignEqStmt;
struct YieldFromStmt;
struct WithStmt;
struct UpdateStmt;
struct PyDefStmt;

struct StarPattern;
struct IntPattern;
struct BoolPattern;
struct StrPattern;
struct SeqPattern;
struct RangePattern;
struct TuplePattern;
struct ListPattern;
struct OrPattern;
struct WildcardPattern;
struct GuardedPattern;
struct BoundPattern;

struct ASTVisitor {
protected:
  virtual void defaultVisit(const Expr *e);
  virtual void defaultVisit(const Stmt *e);
  virtual void defaultVisit(const Pattern *e);

public:
  virtual void visit(const NoneExpr *);
  virtual void visit(const BoolExpr *);
  virtual void visit(const IntExpr *);
  virtual void visit(const FloatExpr *);
  virtual void visit(const StringExpr *);
  virtual void visit(const FStringExpr *);
  virtual void visit(const KmerExpr *);
  virtual void visit(const SeqExpr *);
  virtual void visit(const IdExpr *);
  virtual void visit(const UnpackExpr *);
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
  virtual void visit(const PtrExpr *);
  virtual void visit(const LambdaExpr *);
  virtual void visit(const YieldExpr *);
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
  virtual void visit(const ExtendStmt *);
  virtual void visit(const ImportStmt *);
  virtual void visit(const ExternImportStmt *);
  virtual void visit(const TryStmt *);
  virtual void visit(const GlobalStmt *);
  virtual void visit(const ThrowStmt *);
  virtual void visit(const FunctionStmt *);
  virtual void visit(const ClassStmt *);
  virtual void visit(const AssignEqStmt *);
  virtual void visit(const YieldFromStmt *);
  virtual void visit(const WithStmt *);
  virtual void visit(const PyDefStmt *);

  virtual void visit(const StarPattern *);
  virtual void visit(const IntPattern *);
  virtual void visit(const BoolPattern *);
  virtual void visit(const StrPattern *);
  virtual void visit(const SeqPattern *);
  virtual void visit(const RangePattern *);
  virtual void visit(const TuplePattern *);
  virtual void visit(const ListPattern *);
  virtual void visit(const OrPattern *);
  virtual void visit(const WildcardPattern *);
  virtual void visit(const GuardedPattern *);
  virtual void visit(const BoundPattern *);
};

template <typename TE, typename TS, typename TP>
struct CallbackASTVisitor : public ASTVisitor, public SrcObject {
  virtual TE transform(const std::unique_ptr<Expr> &e) = 0;
  virtual TS transform(const std::unique_ptr<Stmt> &e) = 0;
  virtual TP transform(const std::unique_ptr<Pattern> &e) = 0;

  // template <typename T, typename... Ts>
  // auto transform(const std::unique_ptr<T> &t, Ts &&... args)
  //     -> decltype(transform(t.get())) {
  //   return transform(t.get(), std::forward<Ts>(args)...);
  // }
  template <typename T> auto transform(const std::vector<T> &ts) {
    std::vector<T> r;
    for (auto &e : ts)
      r.push_back(transform(e));
    return r;
  }

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
