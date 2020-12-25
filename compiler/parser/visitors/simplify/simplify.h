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
  /// statement. Useful for prepending lambda function definition that are induced by a
  /// child expression node. Pointer is needed as visitors can be spawned recursively.
  shared_ptr<vector<StmtPtr>> prependStmts;

  /// Simplification step will divide the input AST into four sub-ASTs that are stored
  /// here:
  ///   - Type (class) signatures
  ///   - Global variable signatures (w/o rhs)
  ///   - Function signatures
  ///   - Top-level statements.
  /// Each of these divisions will be populated via first-come first-serve method.
  /// This way, type and global signatures will be exposed to all executable statements,
  /// and we can assume that there won't be any back-references (any signatures depends
  /// only on the previously seen signatures). We also won't have to maintain complex
  /// structures to access global variables, or worry about recursive imports.
  /// This approach also allows us to generate global types without having to
  /// worry about initialization order.
  struct Preamble {
    vector<StmtPtr> types;
    vector<StmtPtr> globals;
    vector<StmtPtr> functions;
  };
  /// Preamble contains shared definition statements and is shared across all visitors
  /// (in all modules). See Preamble (type) for more details.
  shared_ptr<Preamble> preamble;

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
  /// @param barebones Set if a bare-bones standard library is used during testing.
  static StmtPtr apply(shared_ptr<Cache> cache, const StmtPtr &node, const string &file,
                       bool barebones = false);

  /// TODO
  static StmtPtr apply(shared_ptr<SimplifyContext> cache, const StmtPtr &node,
                       const string &file);

public:
  explicit SimplifyVisitor(shared_ptr<SimplifyContext> ctx,
                           shared_ptr<Preamble> preamble,
                           shared_ptr<vector<StmtPtr>> prepend = nullptr);

  /// Transform an AST expression node.
  /// @raise ParserException if a node describes a type (use transformType instead).
  ExprPtr transform(const ExprPtr &expr) override;
  /// Transform an AST statement node.
  StmtPtr transform(const StmtPtr &stmt) override;
  /// Transform an AST pattern node.
  PatternPtr transform(const PatternPtr &pattern) override;
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
  /// Transform None to:
  ///   Optional().
  void visit(const NoneExpr *) override;
  /// See transformInt for details of integer transformation.
  void visit(const IntExpr *) override;
  /// String is transformed if it is an F-string or a custom-prefix string.
  /// Custom prefix strings are transformed to:
  ///   pfx'foo' -> str.__prefix_pfx__[len(foo)]('foo').
  /// For F-strings: see parseFString.
  void visit(const StringExpr *) override;
  /// Each identifier is replaced with its canonical identifier.
  void visit(const IdExpr *) override;
  /// Transform a star-expression *args to:
  ///   List(args.__iter__()).
  /// This function is called only if other nodes (CallExpr, AssignStmt, ListExpr) do
  /// not handle their star-expression cases.
  void visit(const StarExpr *) override;
  /// Transform a tuple (a1, ..., aN) to:
  ///   Tuple.N.__new__(a1, ..., aN).
  /// If Tuple.N has not been seen before, generate a stub class for it.
  void visit(const TupleExpr *) override;
  /// Transform a list [a1, ..., aN] to a statement expression:
  ///   list = List(N); (list.append(a1))...; list.
  /// Any star-expression is automatically unrolled:
  ///   [a, *b] becomes list.append(a); for it in b: list.append(it).
  void visit(const ListExpr *) override;
  /// Transform a set {a1, ..., aN} to a statement expression:
  ///   set = Set(); (set.add(a1))...; set.
  /// Any star-expression is automatically unrolled:
  ///   {a, *b} becomes set.add(a); for it in b: set.add(it).
  void visit(const SetExpr *) override;
  /// Transform a dictionary {k1: v1, ..., kN: vN} to a statement expression
  ///   dict = Dict(); (dict.__setitem__(k1, v1))...; dict.
  /// TODO: allow dictionary unpacking (**dict) (needs parser support).
  void visit(const DictExpr *) override;
  /// Transform a list comprehension [i+a for i in j if a] to a statement expression:
  ///    gen = List()
  ///    for i in j: if a: gen.append(i+a)
  /// Analogous for sets and other comprehension cases.
  /// Also transform a generator (i+a for i in j if a) to a lambda call:
  ///    def _lambda(j, a): for i in j: yield i+a
  ///    _lambda(j, a).__iter__()
  ///
  /// TODO: add list.__len__() optimization.
  void visit(const GeneratorExpr *) override;
  /// Transform a dictionary comprehension [i: a for i in j] to a statement expression:
  ///    gen = Dict()
  ///    for i in j: gen.__setitem__(i, a)
  void visit(const DictGeneratorExpr *) override;
  /// Transform a if-expression a if cond else b to:
  ///   a if cond.__bool__() else b
  void visit(const IfExpr *) override;
  /// Transform a unary expression to the corresponding magic call
  /// (__invert__, __pos__ or __neg__).
  /// Special case: not a is transformed to
  ///   a.__bool__().__invert__()
  void visit(const UnaryExpr *) override;
  /// Transform the following binary expressions:
  ///   a and.or b -> a.__bool__() and/or b.__bool__()
  ///   a is not b -> (a is b).__invert__()
  ///   a not in b -> not (a in b)
  ///   a in b -> a.__contains__(b)
  ///   None is None -> True
  ///   None is b -> b is None.
  /// Other cases are handled during the type-checking stage.
  void visit(const BinaryExpr *) override;
  /// Pipes will be handled during the type-checking stage.
  void visit(const PipeExpr *) override;
  /// Perform the following transformations:
  ///   tuple[T1, ... TN],
  ///   Tuple[T1, ... TN] -> Tuple.N(T1, ..., TN)
  ///     (and generate class Tuple.N)
  ///   function[R, T1, ... TN],
  ///   Function[R, T1, ... TN] -> Function.N(R, T1, ..., TN)
  ///     (and generate class Function.N)
  /// Otherwise, if the index expression is a type instantiation, convert it to an
  /// InstantiateExpr. All other cases are handled during the type-checking stage.
  void visit(const IndexExpr *) override;
  /// Perform the following transformations:
  ///   __ptr__(v) -> PtrExpr(v)
  ///   __array__[T](sz) -> StackAllocExpr(T, sz)
  /// All other cases are handled during the type-checking stage.
  ///
  /// Also generate Tuple.N (if a call has N arguments) to allow passing arguments as a
  /// tuple to Python methods later on.
  void visit(const CallExpr *) override;
  /// Perform the import flattening transformation:
  ///   a.b.c becomes canonical name of c in a.b if a.b is an import
  ///   a.B.c becomes canonical name of c in class a.B
  /// Other cases are handled during the type-checking stage.
  void visit(const DotExpr *) override;
  // This expression is transformed during the type-checking stage
  // because we need raw SliceExpr to handle static tuple slicing.
  void visit(const SliceExpr *) override;
  /// TypeOf expressions will be handled during the type-checking stage.
  void visit(const TypeOfExpr *) override;
  /// Transform lambda a, b: a+b+c to:
  ///   def _lambda(a, b, c): return a+b+c
  ///   _lambda(..., ..., c)
  void visit(const LambdaExpr *) override;
  /// Transform var := expr to a statement expression:
  ///   var = expr; var
  /// Disallowed in dependent parts of short-circuiting expressions
  /// (i.e. b and b2 in "a and b", "a or b" or "b if cond else b2").
  void visit(const AssignExpr *) override;

  /// Transform all statements in a suite and flatten them (unless a suite is a variable
  /// scope).
  void visit(const SuiteStmt *) override;
  /// Ensure that a continue is in a loop.
  void visit(const ContinueStmt *) override;
  /// Ensure that a break is in a loop.
  /// If a loop break variable is available (loop-else block), transform a break to:
  ///   loop_var = false; break
  void visit(const BreakStmt *) override;
  void visit(const ExprStmt *) override;
  /// Performs assignment and unpacking transformations.
  /// See parseAssignment() and unpackAssignments() for more details.
  void visit(const AssignStmt *) override;
  /// Transform del a[x] to:
  ///   del a -> a = typeof(a)() (and removes a from the context)
  ///   del a[x] -> a.__delitem__(x)
  void visit(const DelStmt *) override;
  /// Transform print a to:
  ///   seq_print(a.__str__())
  void visit(const PrintStmt *) override;
  /// Ensure that a return is in a function.
  void visit(const ReturnStmt *) override;
  /// Ensure that a yield is in a function.
  void visit(const YieldStmt *) override;
  /// Transform yield from a to:
  ///   for var in a: yield var
  void visit(const YieldFromStmt *) override;
  /// Transform assert foo(), "message" to:
  ///   if not foo(): seq_assert(<file>, <line>, "message")
  /// Transform assert foo() to:
  ///   if not foo(): seq_assert(<file>, <line>, "")
  /// If simplification stage is invoked during unit testing, call seq_assert_test()
  /// instead.
  void visit(const AssertStmt *) override;
  /// Transform while cond to:
  ///   while cond.__bool__()
  /// Transform while cond: ... else: ... to:
  ///   no_break = True
  ///   while cond.__bool__(): ...
  ///   if no_break.__bool__(): ...
  void visit(const WhileStmt *) override;
  /// Transform for i in it: ... to:
  ///   for i in it.__iter__(): ...
  /// Transform for i, j in it: ... to:
  ///   for tmp in it.__iter__():
  ///      i, j = tmp; ...
  /// This transformation uses AssignStmt and supports all unpack operations that are
  /// handled there.
  /// Transform for i in it: ... else: ... to:
  ///   no_break = True
  ///   for i in it.__iter__(): ...
  ///   if no_break.__bool__(): ...
  void visit(const ForStmt *) override;
  /// Transforms all if conditions to condition.__bool__().
  void visit(const IfStmt *) override;
  void visit(const MatchStmt *) override;
  void visit(const TryStmt *) override;
  void visit(const ThrowStmt *) override;
  /// Transform with foo(), bar() as a: ... to:
  ///   block:
  ///     tmp = foo()
  ///     tmp.__enter__()
  ///     try:
  ///       a = bar()
  ///       a.__enter__()
  ///       try:
  ///         ...
  ///       finally:
  ///         a.__exit__()
  ///     finally:
  ///       tmp.__exit__()
  void visit(const WithStmt *) override;
  /// Perform the global checks and remove the statement from the consideration.
  void visit(const GlobalStmt *) override;
  /// Import a module into its own context.
  /// Unless we are loading the standard library, each import statement is replaced
  /// with:
  ///   if not _import_N_done:
  ///     _import_N()
  ///     _import_N_done = True
  /// to make sure that the first _executed_ import statement executes its statements
  /// (like Python). See parseNewImport() and below for details.
  ///
  /// This function also handles FFI imports (C, Python etc). For the details, see
  /// parseCImport(), parseCDylibImport() and parsePythonImport().
  ///
  /// ⚠️ Warning: This behavior is slightly different than Python's
  /// behavior that executes imports when they are _executed_ first.
  void visit(const ImportStmt *) override;
  /// Transforms function definitions.
  ///
  /// At this stage, the only meaningful transformation happens for "self" arguments in
  /// a class method that have no type annotation (they will get one automatically).
  ///
  /// For Python and LLVM definition transformations, see parsePythonDefinition() and
  /// parseLLVMDefinition().
  void visit(const FunctionStmt *) override;
  /// Transforms type definitions and extensions.
  /// This currently consists of adding default magic methods (described in
  /// codegenMagic() method below).
  void visit(const ClassStmt *) override;

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
  StmtPtr parseCImport(const string &name, const vector<Param> &args, const Expr *ret,
                       const string &altName);
  /// Transform a dynamic C import (from C import lib.foo(int) -> float as f) to:
  ///   def foo(a1: int) -> float:
  ///     fptr = _dlsym(lib, "foo")
  ///     f = Function[float, int](fptr)
  ///     return f(a1)  (if return type is void, just call f(a1))
  StmtPtr parseCDylibImport(const Expr *dylib, const string &name,
                            const vector<Param> &args, const Expr *ret,
                            const string &altName);
  /// Transform a Python module import (from python import module as f) to:
  ///   f = pyobj._import("module")
  /// Transform a Python function import (from python import lib.foo(int) -> float as f)
  /// to:
  ///   def f(a0: int) -> float:
  ///     f = pyobj._import("lib")._getattr("foo")
  ///     return float.__from_py__(f(a0))
  /// If a return type is nullptr, the function just returns f (raw pyobj).
  StmtPtr parsePythonImport(const Expr *what, const vector<Param> &args,
                            const Expr *ret, const string &altName);
  /// Import a new file (with a given module name) into its own context and wrap its
  /// top-level statements into a function to support Python-style runtime import
  /// loading. Once import is done, generate:
  ///   _import_N_done = False
  ///   def _import_N():
  ///     global <imported global variables>...
  ///     __name__ = moduleName
  ///     <imported top-level statements>.
  void parseNewImport(const string &file, const string &moduleName);
  /// Transform a Python code-block @python def foo(x: int, y) -> int: <python code> to:
  ///   pyobj._exec("def foo(x, y): <python code>")
  ///   from python import __main__.foo(int, _) -> int
  StmtPtr parsePythonDefinition(const string &name, const vector<Param> &args,
                                const Expr *ret, const Stmt *codeStmt);
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
