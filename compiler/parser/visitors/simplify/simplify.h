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
 *  - Variardic classes (Tuple.N and Function.N) are generated.
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
  /// @param barebones Set if barebones standard library is used during testing.
  static StmtPtr apply(shared_ptr<Cache> cache, const StmtPtr &node, const string &file,
                       bool barebones = false);

public:
  explicit SimplifyVisitor(shared_ptr<SimplifyContext> ctx,
                           shared_ptr<vector<StmtPtr>> stmts = nullptr);

  /// Transform an AST expression node.
  /// @raise ParserException if a node describes a type (use transformType instead).
  ExprPtr transform(const ExprPtr &expr) override;
  /// Transform an AST statement node.
  StmtPtr transform(const StmtPtr &stmt) override;
  /// Transform an AST pattern node.
  PatternPtr transform(const PatternPtr &pattern) override;
  /// Transform an AST expression node (pointer convenience method).
  ExprPtr transform(const Expr *expr);
  /// Transform an AST statement node (pointer convenience method).
  StmtPtr transform(const Stmt *stmt);
  /// Transform an AST expression node.
  ExprPtr transform(const Expr *expr, bool allowTypes = false);
  /// Transform an AST type expression node.
  /// @raise ParserException if a node does not describe a type (use transform instead).
  ExprPtr transformType(const Expr *expr);

private:
  /// These functions just clone a given node (nothing to be simplified).
  void defaultVisit(const Expr *e) override;
  void defaultVisit(const Stmt *s) override;
  void defaultVisit(const Pattern *p) override;

public:
  /// The following visitors are documented in a corresponding implementation file.
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
  /// constructor:
  ///   123u -> UInt[64](123)
  ///   123i56 -> Int[56]("123")  (same for UInt)
  ///   123pf -> int.__suffix_pf__("123")
  ExprPtr transformInt(const string &value, const string &suffix);
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
  /// Transform an index expression: allow for type expressions, and check if an
  /// expression is a static expression. If so, return StaticExpr.
  ExprPtr transformIndexExpr(const ExprPtr &expr);
  /// Returns true if an expression is compile-time static expression.
  /// Such expression is of a form:
  ///   an integer (IntExpr) without any suffix that is within i64 range
  ///   a static generic
  ///   [-,not] a
  ///   a [+,-,*,//,%,and,or,==,!=,<,<=,>,>=] b
  ///     (note: and/or will NOT short-circuit)
  ///   a if cond else b
  ///     (note: cond is static, and is true if non-zero, false otherwise).
  ///     (note: both branches will be evaluated).
  /// All static generics will be captured and stored in captures out-parameter.
  bool isStaticExpr(const Expr *expr, set<string> &captures);
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

  /// Transforms a simple assignment:
  ///   a[x] = b -> a.__setitem__(x, b)
  ///   a.x = b -> AssignMemberStmt
  ///   a : type = b -> AssignStmt
  ///   a = b -> AssignStmt or UpdateStmt if a exists in the same scope (or is global)
  StmtPtr parseAssignment(const Expr *lhs, const Expr *rhs, const Expr *type,
                          bool shadow, bool mustExist);
  /// Unpack an assignment expression lhs = rhs into a list of simple assignment
  /// expressions (either a = b, or a.x = b, or a[x] = b).
  /// Used to handle various Python unpacking rules, such as:
  ///   (a, b) = c
  ///   a, b = c
  ///   [a, *x, b] = c.
  /// Non-trivial right-hand expressions are first stored in a temporary variable:
  ///   a, b = c, d + foo() -> tmp = (c, d + foo); a = tmp[0]; b = tmp[1].
  /// Processes each assignment recursively to support cases like:
  ///   a, (b, c) = d
  void unpackAssignments(const Expr *lhs, const Expr *rhs, vector<StmtPtr> &stmts,
                         bool shadow, bool mustExist);
  /// Transform a C import (from C import foo(int) -> float as f) to:
  ///   @.c
  ///   def foo(a1: int) -> float: pass
  ///   f = foo (only if altName is provided).
  StmtPtr parseCImport(const string &name, const vector<Param> &args,
                       const ExprPtr &ret, const string &altName);
  /// Transform a dynamic C import (from C import lib.foo(int) -> float as f) to:
  ///   def foo(a1: int) -> float:
  ///     fptr = _dlsym(lib, "foo")
  ///     f = Function[float, int](fptr)
  ///     return f(a1)  (if return type is void, just call f(a1))
  StmtPtr parseCDylibImport(const ExprPtr &dylib, const string &name,
                            const vector<Param> &args, const ExprPtr &ret,
                            const string &altName);
  StmtPtr parsePythonImport(const ExprPtr &what, const string &as);
  /// Transform Python code @python def foo(x): <python code> to:
  ///   _py_exec("def foo(x): <python code>")
  ///   from python import foo
  StmtPtr parsePythonDefinition(const string &name, const vector<Param> &args,
                                const Stmt *codeStmt);
  /// Transform LLVM code @llvm def foo(x: int) -> float: <llvm code> to:
  ///   def foo(x: int) -> float:
  ///     StringExpr("<llvm code>")
  ///     SuiteStmt(referenced_types)
  /// As LLVM code can reference types and static expressions in {= expr} block,
  /// all such referenced expression will be stored in the above referenced_types.
  /// "<llvm code>" will also be transformed accordingly: each {= expr} reference will
  /// be replaced with {} so that fmt::format can easily later fill the gaps.
  /// Note that any brace ({ or }) that is not part of {= expr} reference will be
  /// escaped (e.g. { -> {{ and } -> }}) so that fmt::format can print them as-is.
  StmtPtr parseLLVMDefinition(const Stmt *codeStmt);
  /// Generate a function type Function.N[TR, T1, ..., TN] as follows:
  ///   @internal @tuple @trait
  ///   class Function.N[TR, T1, ..., TN]:
  ///     @internal
  ///     def __new__(what: Ptr[byte]) -> Function.N[TR, T1, ..., TN]: pass
  ///     @internal
  ///     def __str__(self: Function.N[TR, T1, ..., TN]) -> str: pass
  /// Return the canonical name of Function.N.
  string generateFunctionStub(int n);
  /// Generate a magic method __op__ for a type described by typExpr and type arguments
  /// args.
  /// Currently able to generate:
  ///   Constructors: __new__, __init__
  ///   Utilities: __raw__, __hash__, __str__
  ///   Iteration: __iter__, __getitem__, __len__, __contains__
  //    Comparisons: __eq__, __ne__, __lt__, __le__, __gt__, __ge__
  //    Pickling: __pickle__, __unpickle__
  //    Python: __to_py__, __from_py__
  StmtPtr codegenMagic(const string &op, const Expr *typExpr, const vector<Param> &args,
                       bool isRecord);
  // Return a list of all function statements within a given class suite. Checks each
  // suite recursively, and assumes that each statement is either a function or a
  // doc-string.
  vector<const Stmt *> getClassMethods(const Stmt *s);
};

} // namespace ast
} // namespace seq
