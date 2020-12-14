/*
 * simplify.h --- AST simplification transformation.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */
#pragma once

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "parser/ast.h"
#include "parser/common.h"
#include "parser/visitors/simplify/simplify_ctx.h"
#include "parser/visitors/visitor.h"

namespace seq {
namespace ast {

/**
 * Visitor that implements the initial AST simplification transformation.
 * In this stage. the following steps are done:
 *  - All imports are flattened making the resulting AST a self-containing (but fairly
 *    large) AST.
 *  - All identifiers are normalized (no two distinct objects share the same name).
 *  - Variardic stubs are generated (e.g. Tuple.N and Function.N classes).
 *  - Any AST node that can be trivially represented as a set of "simpler" nodes
 *    type is transformed accordingly. If a transformation requires a type information,
 *    it is delayed until the next transformation stage (type-checking).
 *
 * ➡️ Note: This visitor *copies* the incoming AST and does not modify it.
 */
class SimplifyVisitor : public CallbackASTVisitor<ExprPtr, StmtPtr, PatternPtr> {
  /// Shared simplification context.
  shared_ptr<SimplifyContext> ctx;
  /// A pointer to a vector of statements that are to be prepended before the current
  /// statement. Useful for prepending a function definitions that are induced by a
  /// child expression node. Pointer is needed as visitors can be spawned recursively.
  shared_ptr<vector<StmtPtr>> prependStmts;

  /// Each new expression is stored here (as visit() does not return anything) and
  /// later returned by a transform() call.
  ExprPtr resultExpr;
  /// Each new statement is stored here (as visit() does not return anything) and
  /// later returned by a transform() call.
  StmtPtr resultStmt;
  /// Each new pattern is stored here (as visit() does not return anything) and
  /// later returned by a transform() call.
  PatternPtr resultPattern;

public:
  /// Static method that applies SimplifyStage on a given AST node.
  /// @param cache Pointer to the shared transformation cache.
  /// @param file Filename of a AST node.
  static StmtPtr apply(shared_ptr<Cache> cache, const StmtPtr &node,
                       const string &file);

public:
  explicit SimplifyVisitor(shared_ptr<SimplifyContext> ctx,
                           shared_ptr<vector<StmtPtr>> stmts = nullptr);

  /// Transforms an AST expression node.
  /// @raise ParserException if a node describes a type (use transformType instead).
  ExprPtr transform(const ExprPtr &e) override;
  /// Transforms an AST statement node.
  StmtPtr transform(const StmtPtr &s) override;
  /// Transforms an AST pattern node.
  PatternPtr transform(const PatternPtr &p) override;
  /// Transforms an AST expression node.
  ExprPtr transform(const ExprPtr &e, bool allowTypes);
  /// Transforms an AST type expression node.
  /// @raise ParserException if a node does not describe a type (use transform instead).
  ExprPtr transformType(const ExprPtr &expr);

private:
  /// These functions just clone a given node (nothing to be simplified).
  void defaultVisit(const Expr *e) override;
  void defaultVisit(const Stmt *s) override;
  void defaultVisit(const Pattern *p) override;

public:
  /// The following visitors are documented in simplify.cpp.
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
  void visit(const YieldFromStmt *) override;
  void visit(const WithStmt *) override;

  void visit(const TuplePattern *) override;
  void visit(const ListPattern *) override;
  void visit(const OrPattern *) override;
  void visit(const WildcardPattern *) override;
  void visit(const GuardedPattern *) override;
  void visit(const BoundPattern *) override;

  using CallbackASTVisitor<ExprPtr, StmtPtr, PatternPtr>::transform;

private:
  /// Converts binary integers (0bXXX), unsigned integers (XXXu), fixed-width integers
  /// (XXXuN and XXXiN), and other suffix integers to a corresponding integer value or a
  /// constructor.
  ExprPtr transformInt(string value, string suffix);
  /// Converts a Python-like F-string (f"foo {x+1} bar") to a concatenation:
  ///   str.cat(["foo ", str(x + 1), " bar"]).
  /// Also supports "{x=}" specifier (that prints the raw expression as well).
  /// @example f"{x+1=}" becomes str.cat(["x+1=", str(x+1)]).
  ExprPtr parseFString(string value);
  /// Generate tuple classes Tuple.N[T1,...,TN](a1: T1, ..., aN: TN) for all
  /// 0 <= len <= N, and return the canonical name of Tuple.len.
  string generateTupleStub(int len);
  /// Transforms a list of GeneratorBody loops to a corresponding set of statements.
  /// @example (for i in j if a for k in i if a if b) becomes:
  ///          for i in j: if a: for k in i: if a: if b: <prev>
  /// @param prev (out-argument) A pointer to the innermost block (suite) where a
  /// comprehension (or generator) expression should reside.
  StmtPtr getGeneratorBlock(const vector<GeneratorBody> &loops, SuiteStmt *&prev);
  /// Make an anonymous function _lambda_XX with provided statements and argument names.
  /// Function definition is prepended to the current statement.
  /// If the statements refer to outer variables, those variables will be captured and
  /// added to the list of arguments. Returns a call expression that calls this
  /// function with captured variables.
  /// @example Given a statement a+b and argument names a, this generates
  ///            def _lambda(a, b): return a+b
  ///          and returns
  ///            _lambda(b).
  ExprPtr makeAnonFn(vector<StmtPtr> &&stmts,
                     const vector<string> &argNames = vector<string>{});

  vector<StmtPtr> addMethods(const StmtPtr &s);
  string generateFunctionStub(int len);
  StmtPtr codegenMagic(const string &op, const ExprPtr &typExpr,
                       const vector<Param> &args, bool isRecord);

  StmtPtr parseCImport(string name, const vector<Param> &args, const ExprPtr &ret,
                       string altName, StringExpr *code = nullptr);
  StmtPtr parseDylibCImport(const ExprPtr &dylib, string name,
                            const vector<Param> &args, const ExprPtr &ret,
                            string altName);
  StmtPtr parsePythonImport(const ExprPtr &what, string as);
  StmtPtr parseLLVMImport(const Stmt *codeStmt);
  bool isStaticExpr(const ExprPtr &expr, set<string> &captures);
  ExprPtr transformGenericExpr(const ExprPtr &expr);
};

} // namespace ast
} // namespace seq
