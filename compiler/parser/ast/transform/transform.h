/**
 * transform.h
 * Type checking AST walker.
 *
 * Simplifies a given AST and generates types for each expression node.
 */

#pragma once

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "parser/ast/ast/ast.h"
#include "parser/ast/transform/transform_ctx.h"
#include "parser/ast/types.h"
#include "parser/common.h"

namespace seq {
namespace ast {

class TransformVisitor : public CallbackASTVisitor<ExprPtr, StmtPtr, PatternPtr> {
  shared_ptr<TransformContext> ctx;
  shared_ptr<vector<StmtPtr>> prependStmts;
  ExprPtr resultExpr;
  StmtPtr resultStmt;
  PatternPtr resultPattern;

private:
  StmtPtr getGeneratorBlock(const vector<GeneratorBody> &loops, SuiteStmt *&prev);
  vector<StmtPtr> addMethods(const StmtPtr &s);
  string generateFunctionStub(int len);
  string generateTupleStub(int len);
  string generatePartialStub(const string &flag);
  StmtPtr codegenMagic(const string &op, const ExprPtr &typExpr,
                       const vector<Param> &args, bool isRecord);
  ExprPtr makeAnonFn(vector<StmtPtr> &&stmts,
                     const vector<string> &vars = vector<string>{});
  ExprPtr transformInt(string value, string suffix);
  ExprPtr parseFString(string value);

  StmtPtr parseCImport(string name, const vector<Param> &args, const ExprPtr &ret,
                       string altName, StringExpr *code = nullptr);
  StmtPtr parseDylibCImport(const ExprPtr &dylib, string name,
                            const vector<Param> &args, const ExprPtr &ret,
                            string altName);
  StmtPtr parsePythonImport(const ExprPtr &what, string as);

  void defaultVisit(const Expr *e) override;
  void defaultVisit(const Stmt *s) override;
  void defaultVisit(const Pattern *p) override;

public:
  TransformVisitor(shared_ptr<TransformContext> ctx,
                   shared_ptr<vector<StmtPtr>> stmts = nullptr);
  static StmtPtr apply(shared_ptr<Cache> cache, StmtPtr s, const string &file);

  ExprPtr transform(const ExprPtr &e) override;
  StmtPtr transform(const StmtPtr &s) override;
  PatternPtr transform(const PatternPtr &p) override;
  ExprPtr transform(const ExprPtr &e, bool allowTypes);
  ExprPtr transformType(const ExprPtr &expr);

public:
  void visit(const NoneExpr *) override;
  void visit(const IntExpr *) override;
  void visit(const StringExpr *) override;
  void visit(const IdExpr *) override;
  void visit(const StarExpr *) override;
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
  void visit(const TypeOfExpr *) override;
  void visit(const LambdaExpr *) override;

  void visit(const SuiteStmt *) override;
  void visit(const ContinueStmt *) override;
  void visit(const BreakStmt *) override;
  void visit(const ExprStmt *) override;
  void visit(const AssignStmt *) override;
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
  void visit(const AssignEqStmt *) override;
  void visit(const YieldFromStmt *) override;
  void visit(const WithStmt *) override;
  void visit(const TuplePattern *) override;
  void visit(const ListPattern *) override;
  void visit(const OrPattern *) override;
  void visit(const WildcardPattern *) override;
  void visit(const GuardedPattern *) override;
  void visit(const BoundPattern *) override;

  using CallbackASTVisitor<ExprPtr, StmtPtr, PatternPtr>::transform;
};

} // namespace ast
} // namespace seq
