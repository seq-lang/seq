/*
 * typecheck.h --- Type inference and type-dependent AST transformations.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */
#pragma once

#include <map>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "parser/ast.h"
#include "parser/common.h"
#include "parser/visitors/format/format.h"
#include "parser/visitors/typecheck/typecheck_ctx.h"
#include "parser/visitors/visitor.h"

namespace seq {
namespace ast {

types::TypePtr operator|=(types::TypePtr &a, const types::TypePtr &b);

class TypecheckVisitor : public CallbackASTVisitor<ExprPtr, StmtPtr> {
  shared_ptr<TypeContext> ctx;
  shared_ptr<vector<StmtPtr>> prependStmts;

  bool allowVoidExpr;

  ExprPtr resultExpr;
  StmtPtr resultStmt;

public:
  static StmtPtr
  apply(shared_ptr<Cache> cache, StmtPtr stmts,
        const unordered_map<string, pair<string, seq_int_t>> &defines = {});

public:
  explicit TypecheckVisitor(shared_ptr<TypeContext> ctx,
                            const shared_ptr<vector<StmtPtr>> &stmts = nullptr);

  /// All of these are non-const in TypeCheck visitor.
  ExprPtr transform(const ExprPtr &e) override;
  StmtPtr transform(const StmtPtr &s) override;
  ExprPtr transform(ExprPtr &e, bool allowTypes, bool allowVoid = false);
  ExprPtr transformType(ExprPtr &expr);

private:
  void defaultVisit(Expr *e) override;
  void defaultVisit(Stmt *s) override;

public:
  /// Set type to bool.
  void visit(BoolExpr *) override;
  /// Set type to int.
  void visit(IntExpr *) override;
  /// Set type to float.
  void visit(FloatExpr *) override;
  /// Set type to str.
  void visit(StringExpr *) override;
  /// Get the type from dictionary. If static variable (e.g. N), evaluate it and
  /// transform the evaluated number to IntExpr.
  /// Correct the identifier with a realized identifier (e.g. replace Id("Ptr") with
  /// Id("Ptr[byte]")).
  void visit(IdExpr *) override;
  /// Set type to the unification of both sides.
  /// Wrap a side with Optional.__new__() if other side is optional.
  /// Also evaluates static if expressions.
  void visit(IfExpr *) override;
  /// Evaluate static unary expressions.
  void visit(UnaryExpr *) override;
  /// See transformBinary() below.
  /// Also evaluates static binary expressions.
  void visit(BinaryExpr *) override;
  /// Type-checks a pipe expression.
  /// Transform a stage CallExpr foo(x) without an ellipsis into:
  ///   foo(..., x).
  /// Transform any non-CallExpr stage foo into a CallExpr stage:
  ///   foo(...).
  /// If necessary, add stages (e.g. unwrap or Optional.__new__) to support function
  // call type adjustments.
  void visit(PipeExpr *) override;
  /// Transform an instantiation Instantiate(foo, {bar, baz}) to a canonical name:
  ///   IdExpr("foo[bar,baz]").
  void visit(InstantiateExpr *) override;
  /// Transform a slice start:stop:step to:
  ///   Slice(start, stop, step).
  /// Start, stop and step parameters can be nullptr; in that case, just use
  /// Optional.__new__() in place of a corresponding parameter.
  void visit(SliceExpr *) override;
  /// Transform an index expression expr[index] to:
  ///   InstantiateExpr(expr, index) if an expr is a function,
  ///   expr.itemN or a sub-tuple if index is static (see transformStaticTupleIndex()),
  ///   or expr.__getitem__(index).
  void visit(IndexExpr *) override;
  /// See transformDot() below.
  void visit(DotExpr *) override;
  /// See transformCall() below.
  void visit(CallExpr *) override;
  /// Type-checks __array__[T](...) with Array[T].
  void visit(StackAllocExpr *) override;
  /// Type-checks it with a new unbound type.
  void visit(EllipsisExpr *) override;
  /// Transform a TypeOf expression typeof(expr) to a canonical name:
  ///   IdExpr("foo[bar,baz]").
  void visit(TypeOfExpr *) override;
  /// Type-checks __ptr__(expr) with Ptr[typeof(T)].
  void visit(PtrExpr *) override;
  /// Unifies a function return type with a Generator[T] where T is a new unbound type.
  /// The expression itself will have type T.
  void visit(YieldExpr *) override;
  /// Use type of an inner expression.
  void visit(StmtExpr *) override;

  void visit(SuiteStmt *) override;
  void visit(PassStmt *) override;
  void visit(BreakStmt *) override;
  void visit(ContinueStmt *) override;
  void visit(ExprStmt *) override;
  void visit(AssignStmt *) override;
  /// Transform an atomic or an in-place statement a += b to:
  ///   a.__iadd__(a, b)
  ///   typeof(a).__atomic_add__(__ptr__(a), b)
  /// Transform an atomic statement a = min(a, b) (and max(a, b)) to:
  ///   typeof(a).__atomic_min__(__ptr__(a), b).
  /// Transform an atomic update a = b to:
  ///   typeof(a).__atomic_xchg__(__ptr__(a), b).
  /// Transformations are performed only if the appropriate magics are available.
  void visit(UpdateStmt *) override;
  /// Transform a.b = c to:
  ///   unwrap(a).b = c
  /// if a is an Optional that does not have field b.
  void visit(AssignMemberStmt *) override;
  void visit(ReturnStmt *) override;
  void visit(YieldStmt *) override;
  void visit(WhileStmt *) override;
  void visit(ForStmt *) override;
  void visit(IfStmt *) override;
  void visit(TryStmt *) override;
  void visit(ThrowStmt *) override;
  /// Parse a function stub and create a corresponding generic function type.
  /// Also realize built-ins and extern C functions.
  void visit(FunctionStmt *) override;
  /// Parse a type stub and create a corresponding generic type.
  void visit(ClassStmt *) override;

  using CallbackASTVisitor<ExprPtr, StmtPtr>::transform;

private:
  /// Attempts to realize a given type and returns a realization or a nullptr if type is
  /// not realizable.
  /// Note: unifies a given type with its realization if the type can be realized.
  types::TypePtr getRealizedType(types::TypePtr &typ);
  /// If a target type is Optional but the type of a given expression is not,
  /// replace the given expression with Optional(expr).
  void wrapOptionalIfNeeded(const types::TypePtr &targetType, ExprPtr &e);
  /// Transforms a binary operation a op b to a corresponding magic method call:
  ///   a.__magic__(b)
  /// Checks for the following methods:
  ///   __atomic_op__ (atomic inline operation: a += b),
  ///   __iop__ (inline operation: a += b),
  ///   __op__, and
  ///   __rop__.
  /// Also checks for the following special cases:
  ///   a and/or b -> preserves BinaryExpr to allow short-circuits later
  ///   a is None -> a.__bool__().__invert__() (if a is Optional), or False
  ///   a is b -> a.__raw__() == b.__raw__() (if a and b share the same reference type),
  ///             or a == b
  /// Return nullptr if no transformation was made.
  ExprPtr transformBinary(BinaryExpr *expr, bool isAtomic = false,
                          bool *noReturn = nullptr);
  /// Given a tuple type and the expression expr[index], check if an index is a static
  /// expression. If so, and if the type of expr is Tuple, statically extract the
  /// specified tuple item (or a sub-tuple if index is a slice).
  /// Supports both StaticExpr and IntExpr as static expressions.
  ExprPtr transformStaticTupleIndex(types::ClassType *tuple, ExprPtr &expr,
                                    ExprPtr &index);
  /// Transforms a DotExpr expr.member to:
  ///   string(realized type of expr) if member is __class__.
  ///   unwrap(expr).member if expr is of type Optional,
  ///   expr._getattr("member") if expr is of type pyobj,
  ///   DotExpr(expr, member) if a member is a class field,
  ///   IdExpr(method_name) if expr.member is a class method, and
  ///   member(expr, ...) partial call if expr.member is an object method.
  /// If args are set, this method will use it to pick the best overloaded method and
  /// return its IdExpr without making the call partial (the rest will be handled by
  /// CallExpr).
  /// If there are multiple valid overloaded methods, pick the first one
  ///   (TODO: improve this).
  /// Return nullptr if no transformation was made.
  ExprPtr transformDot(DotExpr *expr, vector<CallExpr::Arg> *args = nullptr);
  /// Deactivate any unbound that was activated during the instantiation of type t.
  void deactivateUnbounds(types::Type *t);
  /// Picks the best method named member in a given type that matches the given argument
  /// types. Prefers methods whose signatures are closer to the given arguments (e.g.
  /// a foo(int) will match (int) better that a generic foo(T)). Also takes care of the
  /// Optional arguments.
  /// If multiple valid methods are found, returns the first one. Returns nullptr if no
  /// methods were found.
  types::FuncTypePtr
  findBestMethod(types::ClassType *typ, const string &member,
                 const vector<std::pair<string, types::TypePtr>> &args);
  /// Reorders a given vector or named arguments (consisting of names and the
  /// corresponding types) according to the signature of a given function.
  /// Returns the reordered vector and an associated reordering score (missing
  /// default arguments' score is half of the present arguments).
  /// Score is -1 if the given arguments cannot be reordered.
  /// TODO: parts of this method are repeated in transformCall().
  std::pair<int, vector<std::pair<string, types::TypePtr>>>
  reorderNamedArgs(const types::FuncType *func,
                   const vector<std::pair<string, types::TypePtr>> &args);
  /// Transform a call expression callee(args...).
  /// Intercepts callees that are expr.dot, expr.dot[T1, T2] etc.
  /// Performs the following transformations:
  ///   Tuple(args...) -> Tuple.__new__(args...) (tuple constructor)
  ///   Class(args...) -> c = Class.__new__(); c.__init__(args...); c (StmtExpr)
  ///   pyobj(args...) -> pyobj(Tuple.__new__(args...))
  ///   obj(args...) -> obj.__call__(args...) (non-functions)
  /// This method will also handle the following use-cases:
  ///   named arguments,
  ///   default arguments,
  ///   default generics, and
  ///   *args (TODO).
  /// Any partial call will be transformed as follows:
  ///   callee(arg1, ...) -> Partial.N10(callee, arg1) (partial creation).
  ///   partial_callee(arg1) -> partial_callee.__call__(arg1) (partial completion)
  /// Arguments are unified with the signature types. The following exceptions are
  /// supported:
  ///   Optional[T] -> T (via unwrap())
  ///   T -> Optional[T] (via Optional.__new__())
  ///   T -> Generator[T] (via T.__iter__())
  ///   Partial[...] -> Function[...].
  ///
  /// Pipe notes: if inType and extraStage are set, this method will use inType as a
  /// pipe ellipsis type. extraStage will be set if an Optional conversion/unwrapping
  /// stage needs to be inserted before the current pipeline stage.
  ///
  /// Static call expressions: the following static expressions are supported:
  ///   isinstance(var, type) -> evaluates to bool
  ///   hasattr(type, string) -> evaluates to bool
  ///   staticlen(var) -> evaluates to int
  ///   compile_error(string) -> raises a compiler error
  ///
  /// Note: This is the most evil method in the whole parser suite. 🤦🏻‍
  ExprPtr transformCall(CallExpr *expr, const types::TypePtr &inType = nullptr,
                        ExprPtr *extraStage = nullptr);
  pair<bool, ExprPtr> transformSpecialCall(CallExpr *expr);
  /// Find all generics on which a given function depends and add them to the context.
  void addFunctionGenerics(const types::FuncType *t);
  /// Generate a partial function type Partial.N01...01 (where 01...01 is a mask
  /// of size N) as follows:
  ///   @tuple @no_total_ordering @no_pickle @no_container @no_python
  ///   class Partial.N01...01[T0, T1, ..., TN]:
  ///     ptr: Function[T0, T1,...,TN]
  ///     aI: TI ... # (if mask[I-1] is zero for I >= 1)
  ///     def __call__(self, aI: TI...) -> T0: # (if mask[I-1] is one for I >= 1)
  ///       return self.ptr(self.a1, a2, self.a3...) # (depending on a mask pattern)
  /// The following partial constructor is added if an oldMask is set:
  ///     def __new_<old_mask>_<mask>(p, aI: TI...) # (if oldMask[I-1] != mask[I-1]):
  ///       return Partial.N<mask>.__new__(self.ptr, self.a1, a2, ...) # (see above)
  string generatePartialStub(const string &mask, const string &oldMask);
  /// Create generic types for type or function generics and add them to the context.
  vector<types::Generic> parseGenerics(const vector<Param> &generics, int level);

private:
  types::TypePtr realize(types::TypePtr typ);
  types::TypePtr realizeType(types::ClassType *typ);
  types::TypePtr realizeFunc(types::FuncType *typ);
  std::pair<int, StmtPtr> inferTypes(StmtPtr &&stmt, bool keepLast = false);
  seq::ir::types::Type *getLLVMType(const types::ClassType *t);
};

} // namespace ast
} // namespace seq
