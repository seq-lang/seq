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
#include "parser/ast/types.h"
#include "parser/ast/visitor.h"
#include "parser/common.h"
#include "parser/context.h"

namespace seq {
namespace ast {

class TransformStmtVisitor;

/// Current identifier table
class TypeContext : public VTable<Type> {
private: /** Naming **/
  /// Current filename
  std::string filename;
  /// Context module (e.g. __main__, sys etc)
  std::string module;
  /// Current name prefix (for functions within classes)
  std::string prefix;

  /// Name counter (how many times we used a name)
  /// Used for generating unique name for each identifier
  /// (e.g. if we have two def foo, one will be known as foo and one as foo.1
  std::unordered_map<std::string, int> moduleNames;
  /// Mapping to canonical names
  /// (each SrcInfo positions maps to a unique canonical name)
  std::unordered_map<seq::SrcInfo, std::string> canonicalNames;

private: /** Lookup **/
  /// Hashtable that determines if an identifier is a type variable
  std::unordered_map<std::string, std::stack<bool>> isType;
  /// Store internal types separately for easier access
  std::unordered_map<std::string, TypePtr> internals;
  /// List of class methods and members
  /// Maps canonical class name to a map of methods and members
  /// and their generalized types
  std::unordered_map<std::string, std::unordered_map<std::string, TypePtr>>
      classMembers;
  std::unordered_map<std::string,
                     std::unordered_map<std::string, shared_ptr<FuncType>>>
      classMethods;

private: /** Type-checking **/
  /// Current type-checking level
  int level;
  /// Current unbound type ID counter.
  /// Each unbound variable must have different ID.
  int unboundCount;
  /// Set of active unbound variables.
  /// If type checking is successful, all of them should be resolved.
  std::set<TypePtr> activeUnbounds;

private: /** Realization **/
  /// Template function ASTs.
  /// Mapping from a canonical function name to a pair of
  /// generalized function type and the untyped function AST.
  std::unordered_map<std::string,
                     std::pair<TypePtr, std::shared_ptr<FunctionStmt>>>
      funcASTs;
  /// Template class ASTs.
  /// Mapping from a canonical class name to a pair of
  /// generalized class type and the untyped class AST.
  std::unordered_map<std::string,
                     std::pair<TypePtr, std::shared_ptr<ClassStmt>>>
      classASTs;
  /// Current function realizations.
  /// Mapping from a canonical function name to a hashtable
  /// of realized and fully type-checked function ASTs.
  std::unordered_map<
      std::string,
      std::unordered_map<std::string, std::shared_ptr<FunctionStmt>>>
      funcRealizations;
  /// Current class realizations.
  /// Mapping from a canonical class name to a hashtable
  /// of realized and fully type-checked class ASTs.
  std::unordered_map<
      std::string, std::unordered_map<std::string, std::shared_ptr<ClassStmt>>>
      classRealizations;

private: /** Function utilities **/
  /// Function parsing helpers: maintain current return type
  TypePtr returnType;
  /// Indicates if a return was seen (to account for procedures)
  bool hasSetReturnType;

  // I am still debating should I provide 1000 getters or setters
  // or just leave the classes below friendly as they are by design
  // rather intimate with this class.
  friend class TransformStmtVisitor;
  friend class TransformExprVisitor;
  friend class FormatStmtVisitor;

public:
  TypeContext(const std::string &filename);
  TypePtr find(const std::string &name, bool *isType = nullptr) const;
  TypePtr findInternal(const std::string &name) const;

  /// add/remove overrides that handle isType flag
  void add(const std::string &name, TypePtr var, bool type = false) {
    map[name].push(var);
    isType[name].push(type);
    stack.top().push_back(name);
  }
  void remove(const std::string &name) override {
    VTable::remove(name);
    auto i = isType.find(name);
    i->second.pop();
    if (!i->second.size()) {
      isType.erase(name);
    }
  }

  /// Get canonical name for a SrcInfo
  std::string getCanonicalName(const seq::SrcInfo &info);
  /// Generate canonical name for a SrcInfo and original class/function name
  std::string getCanonicalName(const std::string &name,
                               const seq::SrcInfo &info);

  /// Type-checking helpers
  void increaseLevel();
  void decreaseLevel();
  std::shared_ptr<LinkType> addUnbound(bool setActive = true);
  /// Calls type->instantiate, but populates the instantiation table
  /// with "parent" type.
  /// Example: for list[T].foo, list[int].foo will populate type of foo so that
  /// the generic T gets mapped to int.
  TypePtr instantiate(TypePtr type);
  TypePtr instantiate(TypePtr type, const vector<pair<int, TypePtr>> &generics);

  /// Getters and setters for the method/member/realization lookup tables
  std::shared_ptr<FuncType> findMethod(const ClassType *type,
                                       const string &method);
  TypePtr findMember(const ClassType *type, const string &member);
  std::vector<std::pair<std::string, const FunctionStmt *>>
  getRealizations(const FunctionStmt *stmt);
};

class TransformExprVisitor : public ExprVisitor {
  TypeContext &ctx;
  ExprPtr result{nullptr};
  TransformStmtVisitor &stmtVisitor;
  friend class TransformStmtVisitor;

public:
  TransformExprVisitor(TypeContext &ctx, TransformStmtVisitor &sv);
  ExprPtr transform(const Expr *e);

  std::vector<ExprPtr> transform(const std::vector<ExprPtr> &e);
  // ...so that I don't have to write separate transform for each
  // unique_ptr<T> case...
  template <typename T>
  auto transform(const std::unique_ptr<T> &t) -> decltype(transform(t.get())) {
    return transform(t.get());
  }

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
  void visit(const CallExpr *) override;
  void visit(const DotExpr *) override;
  void visit(const SliceExpr *) override;
  void visit(const EllipsisExpr *) override;
  void visit(const TypeOfExpr *) override;
  void visit(const PtrExpr *) override;
  void visit(const LambdaExpr *) override;
  void visit(const YieldExpr *) override;
};

class TransformStmtVisitor : public StmtVisitor {
  TypeContext &ctx;
  std::vector<StmtPtr> prependStmts;
  StmtPtr result{nullptr};

  /// Helper function that handles simple assignments
  /// (e.g. a = b, a.x = b or a[x] = b)
  StmtPtr addAssignment(const Expr *lhs, const Expr *rhs,
                        const Expr *type = nullptr, bool force = false);
  /// Helper function that decomposes complex assignments into simple ones
  /// (e.g. a, *b, (c, d) = foo)
  void processAssignment(const Expr *lhs, const Expr *rhs,
                         std::vector<StmtPtr> &stmts, bool force = false);

public:
  TransformStmtVisitor(TypeContext &ctx) : ctx(ctx) {}
  void prepend(StmtPtr s);

  void realize(FuncType *type);
  StmtPtr realizeBlock(const Stmt *stmt);

  StmtPtr transform(const Stmt *stmt);
  ExprPtr transform(const Expr *stmt);
  PatternPtr transform(const Pattern *stmt);
  // ...so that I don't have to write separate transform for each
  // unique_ptr<T> case...
  template <typename T>
  auto transform(const std::unique_ptr<T> &t) -> decltype(transform(t.get())) {
    return transform(t.get());
  }

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
  // virtual void visit(const TypeAliasStmt *) override;
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
};

class TransformPatternVisitor : public PatternVisitor {
  TransformStmtVisitor &stmtVisitor;
  PatternPtr result;
  friend TransformStmtVisitor;

public:
  TransformPatternVisitor(TransformStmtVisitor &);
  PatternPtr transform(const Pattern *ptr);
  std::vector<PatternPtr> transform(const std::vector<PatternPtr> &pats);

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
