/*
 * simplify_expr.cpp --- AST expression simplifications.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */
#include <deque>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "parser/ast.h"
#include "parser/cache.h"
#include "parser/common.h"
#include "parser/ocaml/ocaml.h"
#include "parser/visitors/simplify/simplify.h"

using fmt::format;
using std::deque;

namespace seq {
namespace ast {

using namespace types;

ExprPtr SimplifyVisitor::transform(const ExprPtr &expr) {
  return transform(expr.get(), false);
}

ExprPtr SimplifyVisitor::transform(const Expr *expr, bool allowTypes) {
  if (!expr)
    return nullptr;
  SimplifyVisitor v(ctx, preamble, prependStmts);
  v.setSrcInfo(expr->getSrcInfo());
  expr->accept(v);
  if (!allowTypes && v.resultExpr && v.resultExpr->isType())
    error("unexpected type expression");
  return move(v.resultExpr);
}

ExprPtr SimplifyVisitor::transformType(const Expr *expr) {
  auto e = transform(expr, true);
  if (e && !e->isType())
    error("expected type expression");
  return e;
}

void SimplifyVisitor::defaultVisit(const Expr *e) { resultExpr = e->clone(); }

/// Transform None to:
///   Optional().
void SimplifyVisitor::visit(const NoneExpr *expr) {
  resultExpr = transform(N<CallExpr>(N<IdExpr>(".Optional")));
}

/// See transformInt for details of integer transformation.
void SimplifyVisitor::visit(const IntExpr *expr) {
  resultExpr = transformInt(expr->value, expr->suffix);
}

/// String is transformed if it is an F-string or a custom-prefix string.
/// Custom prefix strings are transformed to:
///   pfx'foo' -> str.__prefix_pfx__[len(foo)]('foo').
/// For F-strings: see parseFString.
void SimplifyVisitor::visit(const StringExpr *expr) {
  if (expr->prefix == "f") {
    /// F-strings
    resultExpr = parseFString(expr->value);
  } else if (!expr->prefix.empty()) {
    /// Custom-prefix strings
    resultExpr = transform(
        N<CallExpr>(N<IndexExpr>(N<DotExpr>(N<IdExpr>(".str"),
                                            format("__prefix_{}__", expr->prefix)),
                                 N<IntExpr>(expr->value.size())),
                    N<StringExpr>(expr->value)));
  } else {
    resultExpr = expr->clone();
  }
}

/// Each identifier is replaced with its canonical identifier.
void SimplifyVisitor::visit(const IdExpr *expr) {
  auto val = ctx->find(expr->value);
  if (!val)
    error("identifier '{}' not found", expr->value);

  // If we are accessing an outer non-global variable, raise an error unless
  // we are capturing variables (in that case capture it).
  bool captured = false;
  if (val->isVar()) {
    if (ctx->getBase() != val->getBase() && !val->isGlobal()) {
      if (!ctx->captures.empty()) {
        captured = true;
        ctx->captures.back().insert(expr->value);
      } else {
        error("cannot access non-global variable '{}'", expr->value);
      }
    }
  }

  // Replace the variable with its canonical name. Do not canonize captured
  // variables (they will be later passed as argument names).
  resultExpr = N<IdExpr>(captured ? expr->value : val->canonicalName);
  // Flag the expression as a type expression if it points to a class name or a generic.
  if (val->isType() && !val->isStatic())
    resultExpr->markType();

  // Check if this variable is coming from an enclosing base; if so, ensure that the
  // current base and all bases between the enclosing base point to the enclosing base.
  for (int i = int(ctx->bases.size()) - 1; i >= 0; i--) {
    if (ctx->bases[i].name == val->getBase()) {
      for (int j = i + 1; j < ctx->bases.size(); j++) {
        ctx->bases[j].parent = std::max(i, ctx->bases[j].parent);
        seqassert(ctx->bases[j].parent < j, "invalid base");
      }
      return;
    }
  }
  // If that is not the case, we are probably having a class accessing its enclosing
  // function variable (generic or other identifier). We do not like that!
  if (!val->getBase().empty())
    error(
        "identifier '{}' not found (classes cannot access outer function identifiers)",
        expr->value);
}

/// Transform a star-expression *args to:
///   List(args.__iter__()).
/// This function is called only if other nodes (CallExpr, AssignStmt, ListExpr) do
/// not handle their star-expression cases.
void SimplifyVisitor::visit(const StarExpr *expr) {
  resultExpr = transform(N<CallExpr>(
      N<IdExpr>(".List"), N<CallExpr>(N<DotExpr>(clone(expr->what), "__iter__"))));
}

/// Transform a tuple (a1, ..., aN) to:
///   Tuple.N.__new__(a1, ..., aN).
/// If Tuple.N has not been seen before, generate a stub class for it.
void SimplifyVisitor::visit(const TupleExpr *expr) {
  auto name = generateTupleStub(expr->items.size());
  resultExpr = transform(
      N<CallExpr>(N<DotExpr>(N<IdExpr>(name), "__new__"), clone(expr->items)));
}

/// Transform a list [a1, ..., aN] to a statement expression:
///   list = List(N); (list.append(a1))...; list.
/// Any star-expression is automatically unrolled:
///   [a, *b] becomes list.append(a); for it in b: list.append(it).
void SimplifyVisitor::visit(const ListExpr *expr) {
  vector<StmtPtr> stmts;
  ExprPtr var = N<IdExpr>(ctx->cache->getTemporaryVar("list"));
  stmts.push_back(transform(N<AssignStmt>(
      clone(var),
      N<CallExpr>(N<IdExpr>(".List"),
                  !expr->items.empty() ? N<IntExpr>(expr->items.size()) : nullptr))));
  for (const auto &it : expr->items) {
    if (auto star = it->getStar()) {
      ExprPtr forVar = N<IdExpr>(ctx->cache->getTemporaryVar("it"));
      stmts.push_back(transform(N<ForStmt>(
          clone(forVar), star->what->clone(),
          N<ExprStmt>(N<CallExpr>(N<DotExpr>(clone(var), "append"), clone(forVar))))));
    } else {
      stmts.push_back(transform(
          N<ExprStmt>(N<CallExpr>(N<DotExpr>(clone(var), "append"), clone(it)))));
    }
  }
  resultExpr = N<StmtExpr>(move(stmts), transform(var));
}

/// Transform a set {a1, ..., aN} to a statement expression:
///   set = Set(); (set.add(a1))...; set.
/// Any star-expression is automatically unrolled:
///   {a, *b} becomes set.add(a); for it in b: set.add(it).
void SimplifyVisitor::visit(const SetExpr *expr) {
  vector<StmtPtr> stmts;
  ExprPtr var = N<IdExpr>(ctx->cache->getTemporaryVar("set"));
  stmts.push_back(transform(N<AssignStmt>(clone(var), N<CallExpr>(N<IdExpr>(".Set")))));
  for (auto &it : expr->items)
    if (auto star = it->getStar()) {
      ExprPtr forVar = N<IdExpr>(ctx->cache->getTemporaryVar("it"));
      stmts.push_back(transform(N<ForStmt>(
          clone(forVar), star->what->clone(),
          N<ExprStmt>(N<CallExpr>(N<DotExpr>(clone(var), "add"), clone(forVar))))));
    } else {
      stmts.push_back(transform(
          N<ExprStmt>(N<CallExpr>(N<DotExpr>(clone(var), "add"), clone(it)))));
    }
  resultExpr = N<StmtExpr>(move(stmts), transform(var));
}

/// Transform a dictionary {k1: v1, ..., kN: vN} to a statement expression
///   dict = Dict(); (dict.__setitem__(k1, v1))...; dict.
///
/// TODO: allow dictionary unpacking (**dict) (needs parser support).
void SimplifyVisitor::visit(const DictExpr *expr) {
  vector<StmtPtr> stmts;
  ExprPtr var = N<IdExpr>(ctx->cache->getTemporaryVar("dict"));
  stmts.push_back(
      transform(N<AssignStmt>(clone(var), N<CallExpr>(N<IdExpr>(".Dict")))));
  for (auto &it : expr->items)
    stmts.push_back(transform(N<ExprStmt>(N<CallExpr>(
        N<DotExpr>(clone(var), "__setitem__"), clone(it.key), clone(it.value)))));
  resultExpr = N<StmtExpr>(move(stmts), transform(var));
}

/// Transform a list comprehension [i+a for i in j if a] to a statement expression:
///    gen = List()
///    for i in j: if a: gen.append(i+a)
/// Analogous for sets and other comprehension cases.
/// Also transform a generator (i+a for i in j if a) to a lambda call:
///    def _lambda(j, a): for i in j: yield i+a
///    _lambda(j, a).__iter__()
///
/// TODO: add list.__len__() optimization.
void SimplifyVisitor::visit(const GeneratorExpr *expr) {
  SuiteStmt *prev;
  auto suite = getGeneratorBlock(expr->loops, prev);

  vector<StmtPtr> stmts;
  ExprPtr var = N<IdExpr>(ctx->cache->getTemporaryVar("gen"));
  if (expr->kind == GeneratorExpr::ListGenerator) {
    stmts.push_back(
        transform(N<AssignStmt>(clone(var), N<CallExpr>(N<IdExpr>(".List")))));
    prev->stmts.push_back(
        N<ExprStmt>(N<CallExpr>(N<DotExpr>(clone(var), "append"), clone(expr->expr))));
    stmts.push_back(transform(suite));
    resultExpr = N<StmtExpr>(move(stmts), transform(var));
  } else if (expr->kind == GeneratorExpr::SetGenerator) {
    stmts.push_back(
        transform(N<AssignStmt>(clone(var), N<CallExpr>(N<IdExpr>(".Set")))));
    prev->stmts.push_back(
        N<ExprStmt>(N<CallExpr>(N<DotExpr>(clone(var), "add"), clone(expr->expr))));
    stmts.push_back(transform(suite));
    resultExpr = N<StmtExpr>(move(stmts), transform(var));
  } else {
    prev->stmts.push_back(N<YieldStmt>(clone(expr->expr)));
    stmts.push_back(move(suite));
    resultExpr =
        transform(N<CallExpr>(N<DotExpr>(makeAnonFn(move(stmts)), "__iter__")));
  }
}

/// Transform a dictionary comprehension [i: a for i in j] to a statement expression:
///    gen = Dict()
///    for i in j: gen.__setitem__(i, a)
void SimplifyVisitor::visit(const DictGeneratorExpr *expr) {
  SuiteStmt *prev;
  auto suite = getGeneratorBlock(expr->loops, prev);

  vector<StmtPtr> stmts;
  ExprPtr var = N<IdExpr>(ctx->cache->getTemporaryVar("gen"));
  stmts.push_back(
      transform(N<AssignStmt>(clone(var), N<CallExpr>(N<IdExpr>(".Dict")))));
  prev->stmts.push_back(N<ExprStmt>(N<CallExpr>(N<DotExpr>(clone(var), "__setitem__"),
                                                clone(expr->key), clone(expr->expr))));
  stmts.push_back(transform(suite));
  resultExpr = N<StmtExpr>(move(stmts), transform(var));
}

/// Transform a if-expression a if cond else b to:
///   a if cond.__bool__() else b
void SimplifyVisitor::visit(const IfExpr *expr) {
  auto cond = transform(N<CallExpr>(N<DotExpr>(clone(expr->cond), "__bool__")));
  auto oldAssign = ctx->canAssign;
  ctx->canAssign = false;
  resultExpr = N<IfExpr>(move(cond), transform(expr->ifexpr), transform(expr->elsexpr));
  ctx->canAssign = oldAssign;
}

/// Transform a unary expression to the corresponding magic call
/// (__invert__, __pos__ or __neg__).
///
/// Special case: not a is transformed to
///   a.__bool__().__invert__()
void SimplifyVisitor::visit(const UnaryExpr *expr) {
  if (expr->op == "!") {
    resultExpr = transform(N<CallExpr>(N<DotExpr>(
        N<CallExpr>(N<DotExpr>(clone(expr->expr), "__bool__")), "__invert__")));
  } else {
    string magic;
    if (expr->op == "~")
      magic = "invert";
    else if (expr->op == "+")
      magic = "pos";
    else if (expr->op == "-")
      magic = "neg";
    else
      error("invalid unary operator '{}'", expr->op);
    magic = format("__{}__", magic);
    resultExpr = transform(N<CallExpr>(N<DotExpr>(clone(expr->expr), magic)));
  }
}

/// Transform the following binary expressions:
///   a and.or b -> a.__bool__() and/or b.__bool__()
///   a is not b -> (a is b).__invert__()
///   a not in b -> not (a in b)
///   a in b -> a.__contains__(b)
///   None is None -> True
///   None is b -> b is None.
/// Other cases are handled during the type-checking stage.
void SimplifyVisitor::visit(const BinaryExpr *expr) {
  if (expr->op == "&&" || expr->op == "||") {
    auto l = transform(N<CallExpr>(N<DotExpr>(clone(expr->lexpr), "__bool__")));
    auto oldAssign = ctx->canAssign;
    ctx->canAssign = false;
    resultExpr = N<BinaryExpr>(
        move(l), expr->op,
        transform(N<CallExpr>(N<DotExpr>(clone(expr->rexpr), "__bool__"))));
    ctx->canAssign = oldAssign;
  } else if (expr->op == "is not") {
    resultExpr = transform(N<CallExpr>(N<DotExpr>(
        N<BinaryExpr>(clone(expr->lexpr), "is", clone(expr->rexpr)), "__invert__")));
  } else if (expr->op == "not in") {
    resultExpr = transform(
        N<UnaryExpr>("!", N<CallExpr>(N<DotExpr>(clone(expr->rexpr), "__contains__"),
                                      clone(expr->lexpr))));
  } else if (expr->op == "in") {
    resultExpr = transform(N<CallExpr>(N<DotExpr>(clone(expr->rexpr), "__contains__"),
                                       clone(expr->lexpr)));
  } else if (expr->op == "is") {
    auto le = expr->lexpr->getNone() ? clone(expr->lexpr) : transform(expr->lexpr);
    auto re = expr->rexpr->getNone() ? clone(expr->rexpr) : transform(expr->rexpr);
    if (expr->lexpr->getNone() && expr->rexpr->getNone())
      resultExpr = N<BoolExpr>(true);
    else if (expr->lexpr->getNone())
      resultExpr = N<BinaryExpr>(move(re), expr->op, move(le));
    else
      resultExpr = N<BinaryExpr>(move(le), expr->op, move(re));
  } else {
    resultExpr = N<BinaryExpr>(transform(expr->lexpr), expr->op, transform(expr->rexpr),
                               expr->inPlace);
  }
}

/// Pipes will be handled during the type-checking stage.
void SimplifyVisitor::visit(const PipeExpr *expr) {
  vector<PipeExpr::Pipe> p;
  for (auto &i : expr->items)
    p.push_back({i.op, transform(i.expr)});
  resultExpr = N<PipeExpr>(move(p));
}

/// Perform the following transformations:
///   tuple[T1, ... TN],
///   Tuple[T1, ... TN] -> Tuple.N(T1, ..., TN)
///     (and generate class Tuple.N)
///   function[R, T1, ... TN],
///   Function[R, T1, ... TN] -> Function.N(R, T1, ..., TN)
///     (and generate class Function.N)
/// Otherwise, if the index expression is a type instantiation, convert it to an
/// InstantiateExpr. All other cases are handled during the type-checking stage.
void SimplifyVisitor::visit(const IndexExpr *expr) {
  ExprPtr e = nullptr;
  // First handle the tuple[] and function[] cases.
  if (expr->expr->isId("tuple") || expr->expr->isId("Tuple")) {
    auto t = expr->index->getTuple();
    auto name = generateTupleStub(t ? int(t->items.size()) : 1);
    e = transformType(N<IdExpr>(name).get());
    e->markType();
  } else if (expr->expr->isId("function") || expr->expr->isId("Function")) {
    auto t = expr->index->getTuple();
    auto name = generateFunctionStub(t ? int(t->items.size()) - 1 : 0);
    e = transformType(N<IdExpr>(name).get());
    e->markType();
  } else {
    e = transform(expr->expr.get(), true);
  }
  // IndexExpr[i1, ..., iN] is internally stored as IndexExpr[TupleExpr[i1, ..., iN]]
  // for N > 1, so make sure to check that case.
  vector<ExprPtr> it;
  if (auto t = expr->index->getTuple())
    for (auto &i : t->items)
      it.push_back(transformIndexExpr(i));
  else
    it.push_back(transformIndexExpr(expr->index));

  // Below we check if this is a proper instantiation expression.
  bool allTypes = true;
  bool hasRealTypes = false; // real types are non-static type expressions
  for (auto &i : it) {
    bool isType = i->isType() || i->getStatic();
    if (i->isType())
      hasRealTypes = true;
    if (!isType)
      allTypes = false;
    if (isType && !allTypes)
      error(i, "invalid type expression");
  }
  if (!allTypes && e->isType())
    error("expected type parameters");
  if (allTypes && e->isType()) {
    resultExpr = N<InstantiateExpr>(move(e), move(it));
    resultExpr->markType();
  } else if (allTypes && hasRealTypes) {
    resultExpr = N<InstantiateExpr>(move(e), move(it));
  } else {
    // For some expressions (e.g. self.foo[N]) we are not yet sure if it is an
    // instantiation or an element access (the expression might be a function and we
    // do not know it yet, and all indices are StaticExpr).
    resultExpr =
        N<IndexExpr>(move(e), it.size() == 1 ? move(it[0]) : N<TupleExpr>(move(it)));
  }
}

/// Perform the following transformations:
///   __ptr__(v) -> PtrExpr(v)
///   __array__[T](sz) -> StackAllocExpr(T, sz)
/// All other cases are handled during the type-checking stage.
///
/// Also generate Tuple.N (if a call has N arguments) to allow passing arguments as a
/// tuple to Python methods later on.
void SimplifyVisitor::visit(const CallExpr *expr) {
  if (expr->expr->isId("__ptr__")) {
    if (expr->args.size() == 1 && expr->args[0].value->getId()) {
      auto v = ctx->find(expr->args[0].value->getId()->value);
      if (v && v->isVar()) {
        resultExpr = N<PtrExpr>(transform(expr->args[0].value));
        return;
      }
    }
    error("__ptr__ only accepts a single argument (variable identifier)");
  }
  if (expr->expr->getIndex() && expr->expr->getIndex()->expr->isId("__array__")) {
    if (expr->args.size() != 1)
      error("__array__ only accepts a single argument (size)");
    resultExpr = N<StackAllocExpr>(transformType(expr->expr->getIndex()->index.get()),
                                   transform(expr->args[0].value));
    return;
  }
  generateTupleStub(expr->args.size());
  vector<CallExpr::Arg> args;
  for (auto &i : expr->args)
    args.push_back({i.name, transform(i.value)});
  resultExpr = N<CallExpr>(transform(expr->expr.get(), true), move(args));
}

/// Perform the import flattening transformation:
///   a.b.c becomes canonical name of c in a.b.
/// Other cases are handled during the type-checking stage.
void SimplifyVisitor::visit(const DotExpr *expr) {
  /// First flatten the imports.
  const Expr *e = expr;
  deque<string> chain;
  while (auto d = e->getDot()) {
    chain.push_front(d->member);
    e = d->expr.get();
  }
  if (auto d = e->getId()) {
    chain.push_front(d->value);

    /// Check if this is a import or a class access:
    /// (import1.import2...).(class1.class2...)?.method?
    int importEnd = 0, itemEnd = 0;
    string importName, itemName;
    shared_ptr<SimplifyItem> val = nullptr;
    for (int i = int(chain.size()) - 1; i >= 0; i--) {
      auto s = join(chain, "/", 0, i + 1);
      val = ctx->find(s);
      if (val && val->isImport()) {
        importName = val->canonicalName;
        importEnd = i + 1;
        break;
      }
    }
    // a.b.c is completely import name
    if (importEnd == chain.size()) {
      resultExpr = N<IdExpr>(importName);
      return;
    }
    auto fctx = importName.empty() ? ctx : ctx->cache->imports[importName].ctx;
    for (int i = int(chain.size()) - 1; i >= importEnd; i--) {
      auto s = join(chain, ".", importEnd, i + 1);
      val = fctx->find(s);
      // Make sure that we access only global imported variables.
      if (val && (importName.empty() || val->isGlobal())) {
        itemName = val->canonicalName;
        itemEnd = i + 1;
        break;
      }
    }
    if (itemName.empty() && importName.empty())
      error("identifier '{}' not found", chain[importEnd]);
    if (itemName.empty())
      error("identifier '{}' not found in {}", chain[importEnd], importName);
    resultExpr = N<IdExpr>(itemName);
    if (importName.empty())
      resultExpr = transform(resultExpr.get(), true);
    if (val->isType())
      resultExpr->markType();
    for (int i = itemEnd; i < chain.size(); i++)
      resultExpr = N<DotExpr>(move(resultExpr), chain[i]);
  } else {
    resultExpr = N<DotExpr>(transform(expr->expr.get(), true), expr->member);
  }
}

// This expression is transformed during the type-checking stage
// because we need raw SliceExpr to handle static tuple slicing.
void SimplifyVisitor::visit(const SliceExpr *expr) {
  resultExpr = N<SliceExpr>(transform(expr->start), transform(expr->stop),
                            transform(expr->step));
}

/// TypeOf expressions will be handled during the type-checking stage.
void SimplifyVisitor::visit(const TypeOfExpr *expr) {
  resultExpr = N<TypeOfExpr>(transform(expr->expr.get(), true));
  resultExpr->markType();
}

/// Transform lambda a, b: a+b+c to:
///   def _lambda(a, b, c): return a+b+c
///   _lambda(..., ..., c)
void SimplifyVisitor::visit(const LambdaExpr *expr) {
  vector<StmtPtr> stmts;
  stmts.push_back(N<ReturnStmt>(clone(expr->expr)));
  auto call_raw = makeAnonFn(move(stmts), expr->vars);
  // OK to const_cast as c is already transformed and only handled here.
  auto call = const_cast<CallExpr *>(call_raw->getCall());
  if (!call) {
    seqassert(call, "bad makeAnonFn return value");
  } else if (!call->args.empty()) {
    // Create a partial call: prepend ... for each lambda argument
    for (int i = 0; i < expr->vars.size(); i++)
      call->args.insert(call->args.begin(), {"", N<EllipsisExpr>()});
    resultExpr = transform(call_raw);
  } else {
    resultExpr = move(call->expr);
  }
}

/// Transform var := expr to a statement expression:
///   var = expr; var
/// Disallowed in dependent parts of short-circuiting expressions
/// (i.e. b and b2 in "a and b", "a or b" or "b if cond else b2").
void SimplifyVisitor::visit(const AssignExpr *expr) {
  seqassert(expr->var->getId(), "only simple assignment expression are supported");
  if (!ctx->canAssign)
    error("assignment expression in a short-circuiting subexpression");
  vector<StmtPtr> s;
  s.push_back(transform(N<AssignStmt>(clone(expr->var), clone(expr->expr))));
  resultExpr =
      transform(N<StmtExpr>(move(s), transform(N<IdExpr>(expr->var->getId()->value))));
}

ExprPtr SimplifyVisitor::transformInt(const string &value, const string &suffix) {
  auto to_int = [](const string &s) {
    if (startswith(s, "0b") || startswith(s, "0B"))
      return std::stoull(s.substr(2), nullptr, 2);
    return std::stoull(s, nullptr, 0);
  };
  try {
    if (suffix.empty())
      return N<IntExpr>(to_int(value));
    /// Unsigned numbers: use UInt[64] for that
    if (suffix == "u")
      return transform(N<CallExpr>(N<IndexExpr>(N<IdExpr>(".UInt"), N<IntExpr>(64)),
                                   N<IntExpr>(to_int(value))));
    /// Fixed-precision numbers (uXXX and iXXX)
    /// NOTE: you cannot use binary (0bXXX) format with those numbers.
    /// TODO: implement non-string constructor for these cases.
    if (suffix[0] == 'u' && isdigit(suffix.substr(1)))
      return transform(N<CallExpr>(
          N<IndexExpr>(N<IdExpr>(".UInt"), N<IntExpr>(std::stoi(suffix.substr(1)))),
          N<StringExpr>(value)));
    if (suffix[0] == 'i' && isdigit(suffix.substr(1)))
      return transform(N<CallExpr>(
          N<IndexExpr>(N<IdExpr>(".Int"), N<IntExpr>(std::stoi(suffix.substr(1)))),
          N<StringExpr>(value)));
  } catch (std::out_of_range &) {
    error("integer {} out of range", value);
  }
  /// Custom suffix sfx: use int.__suffix_sfx__(str) call.
  /// NOTE: you cannot neither use binary (0bXXX) format here.
  return transform(
      N<CallExpr>(N<DotExpr>(N<IdExpr>(".int"), format("__suffix_{}__", suffix)),
                  N<StringExpr>(value)));
}

ExprPtr SimplifyVisitor::parseFString(string value) {
  vector<ExprPtr> items;
  int braceCount = 0, braceStart = 0;
  for (int i = 0; i < value.size(); i++) {
    if (value[i] == '{') {
      if (braceStart < i)
        items.push_back(N<StringExpr>(value.substr(braceStart, i - braceStart)));
      if (!braceCount)
        braceStart = i + 1;
      braceCount++;
    } else if (value[i] == '}') {
      braceCount--;
      if (!braceCount) {
        string code = value.substr(braceStart, i - braceStart);
        auto offset = getSrcInfo();
        offset.col += i;
        if (!code.empty() && code.back() == '=') {
          code = code.substr(0, code.size() - 1);
          items.push_back(N<StringExpr>(format("{}=", code)));
        }
        items.push_back(N<CallExpr>(N<IdExpr>("str"), parseExpr(code, offset)));
      }
      braceStart = i + 1;
    }
  }
  if (braceCount)
    error("f-string braces are not balanced");
  if (braceStart != value.size())
    items.push_back(N<StringExpr>(value.substr(braceStart, value.size() - braceStart)));
  return transform(
      N<CallExpr>(N<DotExpr>(N<IdExpr>("str"), "cat"), N<ListExpr>(move(items))));
}

string SimplifyVisitor::generateTupleStub(int len) {
  // Generate all tuples Tuple.(0...len) (to ensure that any slice results in a valid
  // type).
  for (int len_i = 0; len_i <= len; len_i++) {
    auto typeName = format("Tuple.{}", len_i);
    if (ctx->cache->variardics.find(typeName) == ctx->cache->variardics.end()) {
      ctx->cache->variardics.insert(typeName);
      vector<Param> generics, args;
      for (int i = 1; i <= len_i; i++) {
        generics.emplace_back(Param{format("T{}", i), nullptr, nullptr});
        args.emplace_back(
            Param{format("a{0}", i), N<IdExpr>(format("T{}", i)), nullptr});
      }
      StmtPtr stmt = make_unique<ClassStmt>(typeName, move(generics), move(args),
                                            nullptr, vector<string>{ATTR_TUPLE});
      stmt->setSrcInfo(ctx->generateSrcInfo());
      // Make sure to generate this in a clean context.
      auto nctx = make_shared<SimplifyContext>(FILE_GENERATED, ctx->cache);
      SimplifyVisitor(nctx, preamble).transform(stmt);
      auto nval = nctx->find(typeName);
      seqassert(nval, "cannot find '{}'", typeName);
      ctx->cache->imports[STDLIB_IMPORT].ctx->addToplevel(typeName, nval);
      ctx->cache->imports[STDLIB_IMPORT].ctx->addToplevel(nval->canonicalName, nval);
    }
  }
  return format(".Tuple.{}", len);
}

StmtPtr SimplifyVisitor::getGeneratorBlock(const vector<GeneratorBody> &loops,
                                           SuiteStmt *&prev) {
  StmtPtr suite = N<SuiteStmt>(), newSuite = nullptr;
  prev = (SuiteStmt *)suite.get();
  for (auto &l : loops) {
    newSuite = N<SuiteStmt>();
    auto nextPrev = (SuiteStmt *)newSuite.get();

    prev->stmts.push_back(N<ForStmt>(l.vars->clone(), l.gen->clone(), move(newSuite)));
    prev = nextPrev;
    for (auto &cond : l.conds) {
      newSuite = N<SuiteStmt>();
      nextPrev = (SuiteStmt *)newSuite.get();
      prev->stmts.push_back(N<IfStmt>(cond->clone(), move(newSuite)));
      prev = nextPrev;
    }
  }
  return suite;
}

ExprPtr SimplifyVisitor::transformIndexExpr(const ExprPtr &expr) {
  auto t = transform(expr.get(), true);
  set<string> captures;
  if (isStaticExpr(t.get(), captures))
    return N<StaticExpr>(clone(t), move(captures));
  else
    return t;
}

bool SimplifyVisitor::isStaticExpr(const Expr *expr, set<string> &captures) {
  static unordered_set<string> supported{"<",  "<=", ">", ">=", "==", "!=", "&&",
                                         "||", "+",  "-", "*",  "//", "%"};
  if (auto ei = expr->getId()) {
    auto val = ctx->find(ei->value);
    if (val && val->isStatic()) {
      captures.insert(ei->value);
      return true;
    }
    return false;
  } else if (auto eb = expr->getBinary()) {
    return (supported.find(eb->op) != supported.end()) &&
           isStaticExpr(eb->lexpr.get(), captures) &&
           isStaticExpr(eb->rexpr.get(), captures);
  } else if (auto eu = expr->getUnary()) {
    return ((eu->op == "-") || (eu->op == "!")) &&
           isStaticExpr(eu->expr.get(), captures);
  } else if (auto ef = expr->getIf()) {
    return isStaticExpr(ef->cond.get(), captures) &&
           isStaticExpr(ef->ifexpr.get(), captures) &&
           isStaticExpr(ef->elsexpr.get(), captures);
  } else if (auto eit = expr->getInt()) {
    if (!eit->suffix.empty())
      return false;
    try {
      std::stoull(eit->value, nullptr, 0);
    } catch (std::out_of_range &) {
      return false;
    }
    return true;
  } else {
    return false;
  }
}

ExprPtr SimplifyVisitor::makeAnonFn(vector<StmtPtr> &&stmts,
                                    const vector<string> &argNames) {
  vector<Param> params;
  vector<CallExpr::Arg> args;

  string name = ctx->cache->getTemporaryVar("lambda", '.');
  ctx->captures.emplace_back(set<string>{});
  for (auto &s : argNames)
    params.emplace_back(Param{s, nullptr, nullptr});
  auto fs = transform(N<FunctionStmt>(name, nullptr, vector<Param>{}, move(params),
                                      N<SuiteStmt>(move(stmts)), vector<string>{}));
  auto f = ctx->cache->functions[name].ast.get(); // name is already canonical!
  for (auto &c : ctx->captures.back()) {
    f->args.emplace_back(Param{c, nullptr, nullptr});
    args.emplace_back(CallExpr::Arg{"", N<IdExpr>(c)});
  }
  if (fs) {
    auto fp = const_cast<FunctionStmt *>(fs->getFunction());
    seqassert(fp, "not a function");
    fp->args = clone_nop(f->args);
    prependStmts->push_back(move(fs));
  }
  ctx->captures.pop_back();
  return N<CallExpr>(N<IdExpr>(name), move(args));
}

} // namespace ast
} // namespace seq
