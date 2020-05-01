/**
 * TODO here:
 * - Finish remaining statements
 * - Handle __iop__/__rop__ magics
 * - Redo error messages (right now they are awful)
 * - (handle pipelines here?)
 * - Fix all TODOs below
 */

#include "util/fmt/format.h"
#include "util/fmt/ostream.h"
#include <deque>
#include <memory>
#include <ostream>
#include <stack>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/ast/ast.h"
#include "parser/ast/transform.h"
#include "parser/ast/typecontext.h"
#include "parser/ast/types.h"
#include "parser/common.h"
#include "parser/ocaml.h"

using fmt::format;
using std::deque;
using std::dynamic_pointer_cast;
using std::get;
using std::make_shared;
using std::make_unique;
using std::move;
using std::ostream;
using std::pair;
using std::shared_ptr;
using std::stack;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::unordered_set;
using std::vector;

int __level__ = 0;
#define CAST(s, T) dynamic_cast<T *>(s.get())
// #define makeBoolExpr(e)                                                             \
//   (Nx<CallExpr>(e.get(), Nx<DotExpr>(e.get(), e->clone(), "__bool__")))

namespace seq {
namespace ast {

template <typename T> void forceUnify(T &expr, TypePtr t) {
  if (expr->getType() && expr->getType()->unify(t) < 0)
    error(expr, "cannot unify {} and {}", *expr->getType(), *t);
}

ExprPtr TransformVisitor::conditionalMagic(const ExprPtr &expr,
                                           const string &type,
                                           const string &magic) {
  auto e = transform(expr);
  if (getUnbound(e->getType()))
    return e;
  if (auto c = getClass(e->getType())) {
    if (c->name == type)
      return e;
    return transform(
        Nx<CallExpr>(e.get(), Nx<DotExpr>(e.get(), move(e), magic)));
  } else {
    error(e, "unexpected expr");
  }
  return nullptr;
}

ExprPtr TransformVisitor::makeBoolExpr(const ExprPtr &e) {
  return conditionalMagic(e, "bool", "__bool__");
}

TransformVisitor::TransformVisitor(TypeContext &ctx,
                                   shared_ptr<vector<StmtPtr>> stmts)
    : ctx(ctx) {
  prependStmts = stmts ? stmts : make_shared<vector<StmtPtr>>();
}

void TransformVisitor::prepend(StmtPtr s) {
  if (auto t = transform(s))
    prependStmts->push_back(move(t));
}

ExprPtr TransformVisitor::transform(const Expr *expr, bool allowTypes) {
  if (!expr)
    return nullptr;
  TransformVisitor v(ctx, prependStmts);
  v.setSrcInfo(expr->getSrcInfo());
  DBG("[ {} :- {} # {}", *expr,
      expr->getType() ? expr->getType()->toString() : "-",
      expr->getSrcInfo().line);
  __level__++;
  expr->accept(v);
  __level__--;
  DBG("  {} :- {} ]", *v.resultExpr,
      v.resultExpr->getType() ? v.resultExpr->getType()->toString() : "-");

  if (v.resultExpr && v.resultExpr->getType() &&
      v.resultExpr->getType()->canRealize()) {
    if (auto c = getClass(v.resultExpr->getType()))
      realize(c);
  }
  if (!allowTypes && v.resultExpr && v.resultExpr->isType())
    error(expr, "unexpected type");
  return move(v.resultExpr);
}

ExprPtr TransformVisitor::transformType(const ExprPtr &expr) {
  auto e = transform(expr.get(), true);
  if (e && !e->isType())
    error(expr, "expected a type");
  return e;
}

StmtPtr TransformVisitor::transform(const Stmt *stmt) {
  if (!stmt)
    return nullptr;

  TransformVisitor v(ctx);
  v.setSrcInfo(stmt->getSrcInfo());

  DBG("{{ {} ## {}", *stmt, stmt->getSrcInfo().line);
  __level__++;
  stmt->accept(v);
  __level__--;
  if (v.prependStmts->size()) {
    if (v.resultStmt)
      v.prependStmts->push_back(move(v.resultStmt));
    v.resultStmt = N<SuiteStmt>(move(*v.prependStmts));
  }
  DBG("  <> {} }}", v.resultStmt ? v.resultStmt->toString() : "#pass");
  return move(v.resultStmt);
}

PatternPtr TransformVisitor::transform(const Pattern *pat) {
  if (!pat)
    return nullptr;
  TransformVisitor v(ctx, prependStmts);
  pat->accept(v);
  return move(v.resultPattern);
}

/*************************************************************************************/

TypeContext::FuncRealization TransformVisitor::realize(FuncTypePtr t) {
  assert(t->canRealize());
  assert(t->name != "");
  auto it = ctx.funcRealizations.find(t->name);
  if (it != ctx.funcRealizations.end()) {
    auto it2 = it->second.find(t->toString(true));
    if (it2 != it->second.end())
      return it2->second;
  }

  ctx.addBlock();
  ctx.increaseLevel();
  // Ensure that all inputs are realized
  for (auto &t : t->args) {
    assert(!t.second->hasUnbound());
    ctx.add(t.first, make_shared<LinkType>(t.second));
  }
  auto old = ctx.returnType;
  auto oldSeen = ctx.hasSetReturnType;
  ctx.returnType = t->ret;
  ctx.hasSetReturnType = false;

  assert(ctx.funcASTs.find(t->name) != ctx.funcASTs.end());
  auto &ast = ctx.funcASTs[t->name];
  // There is no AST linked to internal functions, so just ignore them
  ctx.bases.push_back(ast.second->name);
  bool isInternal =
      std::find(ast.second->attributes.begin(), ast.second->attributes.end(),
                "internal") != ast.second->attributes.end();
  auto realized = isInternal ? nullptr : realizeBlock(ast.second->suite.get());
  ctx.bases.pop_back();

  // DBG("======== BEGIN {} :- {} ========", t->name, *t);
  if (realized && !ctx.hasSetReturnType && t->ret) {
    auto u = ctx.returnType->unify(ctx.findInternal("void"));
    assert(u >= 0);
  }
  assert(t->ret->canRealize());
  // DBG("======== END {} :- {} ========", t->name, *t);

  assert(ast.second->args.size() == t->args.size());
  vector<Param> args;
  for (auto &i : ast.second->args)
    args.push_back({i.name, nullptr, nullptr});
  DBG("<:> {} {}", t->name, t->toString(true));
  auto ret = ctx.funcRealizations[t->name][t->toString(true)] = {
      t,
      Nx<FunctionStmt>(ast.second.get(), t->name, nullptr, vector<string>(),
                       move(args), move(realized), ast.second->attributes),
      nullptr};
  ctx.returnType = old;
  ctx.hasSetReturnType = oldSeen;
  ctx.decreaseLevel();
  ctx.popBlock();
  DBG(">> realized {}::{}", t->name, *t);
  return ret;
}

TypeContext::ClassRealization TransformVisitor::realize(ClassTypePtr t) {
  assert(t->canRealize());
  auto it = ctx.classRealizations.find(t->name);
  if (it != ctx.classRealizations.end()) {
    auto it2 = it->second.find(t->toString(true));
    if (it2 != it->second.end())
      return it2->second;
  }

  types::Type *handle = nullptr;
  if (t->isRecord) {
    vector<string> names;
    vector<types::Type *> types;
    for (auto &m : t->args) {
      names.push_back(m.first);
      assert(m.second->canRealize() && getClass(m.second));
      auto real = realize(getClass(m.second));
      types.push_back(real.handle);
    }
    handle =
        types::RecordType::get(types, names, t->name == "tuple" ? "" : t->name);
  } else {
    auto cls = types::RefType::get(t->name);
    vector<string> names;
    vector<types::Type *> types;
    for (auto &m : ctx.classes[t->name].members) {
      names.push_back(m.first);
      auto mt = ctx.instantiate(t->getSrcInfo(), m.second, t->generics);
      assert(mt->canRealize() && getClass(mt));
      auto real = realize(getClass(mt));
      types.push_back(real.handle);
    }
    cls->setContents(types::RecordType::get(types, names, ""));
    cls->setDone();
    handle = cls;
  }
  DBG(">> realized {}", *t);
  return ctx.classRealizations[t->name][t->toString(true)] = {t, handle};
}

StmtPtr TransformVisitor::realizeBlock(const Stmt *stmt) {
  if (!stmt) {
    return nullptr;
  }
  StmtPtr result = nullptr;

  FILE *fo = fopen("out.htm", "w");
  // We keep running typecheck transformations until there are no more unbound
  // types. It is assumed that the unbound count will decrease in each
  // iteration--- if not, the program cannot be type-checked.
  // TODO: this can be probably optimized one day...
  int reachSize = ctx.activeUnbounds.size();
  for (int iter = 0, prevSize = INT_MAX; prevSize > reachSize; iter++) {
    DBG("---------------------------------------- ROUND {} # {}", iter,
        reachSize);
    TransformVisitor v(ctx);
    result = v.transform(result ? result.get() : stmt);

    for (auto i = ctx.activeUnbounds.begin(); i != ctx.activeUnbounds.end();) {
      if (auto l = dynamic_pointer_cast<LinkType>(*i)) {
        if (l->kind != LinkType::Unbound) {
          i = ctx.activeUnbounds.erase(i);
          continue;
        }
      }
      ++i;
    }
    DBG("post {}", ctx.activeUnbounds.size());
    if (ctx.activeUnbounds.size() >= prevSize) {
      for (auto &ub : ctx.activeUnbounds)
        DBG("NOPE {}", (*ub));
      error("cannot resolve unbound variables");
      break;
    }
    prevSize = ctx.activeUnbounds.size();
  }
  fmt::print(fo, "{}", ast::FormatVisitor::format(ctx, result, true));
  fclose(fo);

  return result;
}

/*************************************************************************************/

void TransformVisitor::visit(const NoneExpr *expr) {
  resultExpr = expr->clone();
  if (!expr->getType())
    resultExpr->setType(ctx.addUnbound(getSrcInfo()));
}

void TransformVisitor::visit(const BoolExpr *expr) {
  resultExpr = expr->clone();
  if (!expr->getType())
    resultExpr->setType(T<LinkType>(ctx.findInternal("bool")));
}

void TransformVisitor::visit(const IntExpr *expr) {
  resultExpr = expr->clone();
  if (!expr->getType())
    resultExpr->setType(T<LinkType>(ctx.findInternal("int")));
}

void TransformVisitor::visit(const FloatExpr *expr) {
  resultExpr = expr->clone();
  if (!expr->getType())
    resultExpr->setType(T<LinkType>(ctx.findInternal("float")));
}

void TransformVisitor::visit(const StringExpr *expr) {
  resultExpr = expr->clone();
  if (!expr->getType())
    resultExpr->setType(T<LinkType>(ctx.findInternal("str")));
}

// Transformed
void TransformVisitor::visit(const FStringExpr *expr) {
  int braceCount = 0, braceStart = 0;
  vector<ExprPtr> items;
  for (int i = 0; i < expr->value.size(); i++) {
    if (expr->value[i] == '{') {
      if (braceStart < i)
        items.push_back(
            N<StringExpr>(expr->value.substr(braceStart, i - braceStart)));
      if (!braceCount)
        braceStart = i + 1;
      braceCount++;
    } else if (expr->value[i] == '}') {
      braceCount--;
      if (!braceCount) {
        string code = expr->value.substr(braceStart, i - braceStart);
        auto offset = expr->getSrcInfo();
        offset.col += i;
        if (code.size() && code.back() == '=') {
          code = code.substr(0, code.size() - 1);
          items.push_back(N<StringExpr>(format("{}=", code)));
        }
        items.push_back(
            N<CallExpr>(N<IdExpr>("str"), parse_expr(code, offset)));
      }
      braceStart = i + 1;
    }
  }
  if (braceCount)
    error(expr, "f-string braces not balanced");
  if (braceStart != expr->value.size())
    items.push_back(N<StringExpr>(
        expr->value.substr(braceStart, expr->value.size() - braceStart)));
  resultExpr = transform(N<CallExpr>(N<DotExpr>(N<IdExpr>("str"), "cat"),
                                     N<ListExpr>(move(items))));
}

// Transformed
void TransformVisitor::visit(const KmerExpr *expr) {
  resultExpr = transform(N<CallExpr>(
      N<IndexExpr>(N<IdExpr>("Kmer"), N<IntExpr>(expr->value.size())),
      N<SeqExpr>(expr->value)));
}

// Transformed
void TransformVisitor::visit(const SeqExpr *expr) {
  if (expr->prefix == "p") {
    resultExpr =
        transform(N<CallExpr>(N<IdExpr>("pseq"), N<StringExpr>(expr->value)));
  } else if (expr->prefix == "s") {
    resultExpr =
        transform(N<CallExpr>(N<IdExpr>("seq"), N<StringExpr>(expr->value)));
  } else {
    error(expr, "invalid seq prefix '{}'", expr->prefix);
  }
}

// TODO globals
void TransformVisitor::visit(const IdExpr *expr) {
  resultExpr = expr->clone();
  if (!expr->getType()) {
    auto val = ctx.find(expr->value);
    if (!val)
      error(expr, "identifier '{}' not found", expr->value);
    if (val->isType())
      resultExpr->markType();
    resultExpr->setType(ctx.instantiate(getSrcInfo(), val->getType()));
  }
}

// Transformed
void TransformVisitor::visit(const UnpackExpr *expr) {
  resultExpr = transform(N<CallExpr>(N<IdExpr>("list"), expr->what->clone()));
}

void TransformVisitor::visit(const TupleExpr *expr) {
  auto e = N<TupleExpr>(transform(expr->items));

  vector<pair<string, TypePtr>> args;
  for (auto &i : e->items)
    args.push_back({"", i->getType()});
  auto t = T<LinkType>(
      T<ClassType>("tuple", true, vector<pair<int, TypePtr>>(), args));
  forceUnify(expr, t);
  e->setType(t);
  resultExpr = move(e);
}

// Transformed
void TransformVisitor::visit(const ListExpr *expr) {
  string listVar = getTemporaryVar("list");
  prepend(N<AssignStmt>(
      N<IdExpr>(listVar),
      N<CallExpr>(N<IdExpr>("list"), expr->items.size()
                                         ? N<IntExpr>(expr->items.size())
                                         : nullptr)));
  for (int i = 0; i < expr->items.size(); i++)
    prepend(N<ExprStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(listVar), "append"),
                                    expr->items[i]->clone())));
  resultExpr = transform(N<IdExpr>(listVar));
}

// Transformed
void TransformVisitor::visit(const SetExpr *expr) {
  string setVar = getTemporaryVar("set");
  prepend(N<AssignStmt>(N<IdExpr>(setVar), N<CallExpr>(N<IdExpr>("set"))));
  for (int i = 0; i < expr->items.size(); i++)
    prepend(N<ExprStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(setVar), "add"),
                                    expr->items[i]->clone())));
  resultExpr = transform(N<IdExpr>(setVar));
}

// Transformed
void TransformVisitor::visit(const DictExpr *expr) {
  string dictVar = getTemporaryVar("dict");
  prepend(N<AssignStmt>(N<IdExpr>(dictVar), N<CallExpr>(N<IdExpr>("dict"))));
  for (int i = 0; i < expr->items.size(); i++)
    prepend(N<ExprStmt>(N<CallExpr>(
        N<DotExpr>(N<IdExpr>(dictVar), "__setitem__"),
        expr->items[i].key->clone(), expr->items[i].value->clone())));
  resultExpr = transform(N<IdExpr>(dictVar));
}

// Transformed
TransformVisitor::CaptureVisitor::CaptureVisitor(TypeContext &ctx) : ctx(ctx) {}

void TransformVisitor::CaptureVisitor::visit(const IdExpr *expr) {
  auto val = ctx.find(expr->value);
  if (!val)
    error(expr, "identifier '{}' not found", expr->value);
  if (!val->isType())
    captures.insert(expr->value);
}

StmtPtr
TransformVisitor::getGeneratorBlock(const vector<GeneratorExpr::Body> &loops,
                                    SuiteStmt *&prev) {
  StmtPtr suite = N<SuiteStmt>(), newSuite = nullptr;
  prev = (SuiteStmt *)suite.get();
  SuiteStmt *nextPrev = nullptr;
  for (auto &l : loops) {
    newSuite = N<SuiteStmt>();
    nextPrev = (SuiteStmt *)newSuite.get();

    vector<ExprPtr> vars;
    for (auto &s : l.vars)
      vars.push_back(N<IdExpr>(s));
    prev->stmts.push_back(
        N<ForStmt>(vars.size() == 1 ? move(vars[0]) : N<TupleExpr>(move(vars)),
                   l.gen->clone(), move(newSuite)));
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

// Transformed
void TransformVisitor::visit(const GeneratorExpr *expr) {
  SuiteStmt *prev;
  auto suite = getGeneratorBlock(expr->loops, prev);
  string var = getTemporaryVar("gen");
  if (expr->kind == GeneratorExpr::ListGenerator) {
    prepend(N<AssignStmt>(N<IdExpr>(var), N<CallExpr>(N<IdExpr>("list"))));
    prev->stmts.push_back(N<ExprStmt>(N<CallExpr>(
        N<DotExpr>(N<IdExpr>(var), "append"), expr->expr->clone())));
    prepend(move(suite));
  } else if (expr->kind == GeneratorExpr::SetGenerator) {
    prepend(N<AssignStmt>(N<IdExpr>(var), N<CallExpr>(N<IdExpr>("set"))));
    prev->stmts.push_back(N<ExprStmt>(N<CallExpr>(
        N<DotExpr>(N<IdExpr>(var), "insert"), expr->expr->clone())));
    prepend(move(suite));
  } else {
    CaptureVisitor cv(ctx);
    expr->expr->accept(cv);

    prev->stmts.push_back(N<YieldStmt>(expr->expr->clone()));
    string fnVar = getTemporaryVar("anonGen");

    vector<Param> captures;
    for (auto &c : cv.captures)
      captures.push_back({c, nullptr, nullptr});
    prepend(N<FunctionStmt>(fnVar, nullptr, vector<string>{}, move(captures),
                            move(suite), vector<string>{}));
    vector<CallExpr::Arg> args;
    for (auto &c : cv.captures)
      args.push_back({c, nullptr});
    prepend(
        N<AssignStmt>(N<IdExpr>(var),
                      N<CallExpr>(N<IdExpr>("iter"),
                                  N<CallExpr>(N<IdExpr>(fnVar), move(args)))));
  }
  resultExpr = transform(N<IdExpr>(var));
}

// Transformed
void TransformVisitor::visit(const DictGeneratorExpr *expr) {
  SuiteStmt *prev;
  auto suite = getGeneratorBlock(expr->loops, prev);
  string var = getTemporaryVar("gen");
  prepend(N<AssignStmt>(N<IdExpr>(var), N<CallExpr>(N<IdExpr>("dict"))));
  prev->stmts.push_back(
      N<ExprStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "__setitem__"),
                              expr->key->clone(), expr->expr->clone())));
  prepend(move(suite));
  resultExpr = transform(N<IdExpr>(var));
}

void TransformVisitor::visit(const IfExpr *expr) {
  auto e = N<IfExpr>(makeBoolExpr(expr->cond), transform(expr->eif),
                     transform(expr->eelse));
  if (e->eif->getType()->unify(e->eelse->getType()) < 0)
    error(expr, "type {} is not compatible with {}",
          e->eif->getType()->toString(), e->eelse->getType()->toString());
  auto t = e->eif->getType();
  forceUnify(expr, t);
  e->setType(t);
  resultExpr = move(e);
}

// Transformed
void TransformVisitor::visit(const UnaryExpr *expr) {
  if (expr->op == "!") { // Special case
    auto e = N<UnaryExpr>(expr->op, makeBoolExpr(expr->expr));
    auto t = T<LinkType>(ctx.findInternal("bool"));
    forceUnify(expr, t);
    e->setType(t);
    resultExpr = move(e);
    return;
  }

  string magic;
  if (expr->op == "~")
    magic = "invert";
  else if (expr->op == "+")
    magic = "pos";
  else if (expr->op == "-")
    magic = "neg";
  else
    error(expr, "invalid unary operator '{}'", expr->op);
  magic = format("__{}__", magic);
  resultExpr = transform(N<CallExpr>(N<DotExpr>(expr->expr->clone(), magic)));
}

// Transformed
void TransformVisitor::visit(const BinaryExpr *expr) {
  if (expr->op == "&&" || expr->op == "||") { // Special case
    auto e = N<BinaryExpr>(makeBoolExpr(expr->lexpr), expr->op,
                           makeBoolExpr(expr->rexpr));
    auto t = T<LinkType>(ctx.findInternal("bool"));
    forceUnify(expr, t);
    e->setType(t);
    resultExpr = move(e);
    return;
  }

  string magic;
  // Maybe use hash table ...
  if (expr->op == "+")
    magic = "add";
  else if (expr->op == "-")
    magic = "sub";
  else if (expr->op == "*")
    magic = "mul";
  else if (expr->op == "**")
    magic = "pow";
  else if (expr->op == "/")
    magic = "truediv";
  else if (expr->op == "//")
    magic = "div";
  else if (expr->op == "@")
    magic = "mathmul";
  else if (expr->op == "%")
    magic = "mod";
  else if (expr->op == "<")
    magic = "lt";
  else if (expr->op == "<=")
    magic = "le";
  else if (expr->op == ">")
    magic = "gt";
  else if (expr->op == ">=")
    magic = "ge";
  else if (expr->op == "==")
    magic = "eq";
  else if (expr->op == "!=")
    magic = "ne";
  else if (expr->op == "<<")
    magic = "lshift";
  else if (expr->op == ">>")
    magic = "rshift";
  else if (expr->op == "&")
    magic = "and";
  else if (expr->op == "|")
    magic = "or";
  else if (expr->op == "^")
    magic = "xor";
  else
    error(expr, "invalid binary operator '{}'", expr->op);
  // TODO: handle iop/rop
  magic = format("__{}__", magic);
  resultExpr = transform(N<CallExpr>(N<DotExpr>(expr->lexpr->clone(), magic),
                                     expr->rexpr->clone()));
}

// TODO
void TransformVisitor::visit(const PipeExpr *expr) {
  error(expr, "to be done later");
  // vector<PipeExpr::Pipe> items;
  // for (auto &l : expr->items) {
  //   items.push_back({l.op, transform(l.expr)});
  // }
  // resultPattern = N<PipeExpr>(move(items));
}

void TransformVisitor::visit(const IndexExpr *expr) {
  // If this is a type or function realization
  // (e.g. dict[type1, type2]), handle it separately

  auto e = transform(expr->expr, true);
  if (e->isType() || getFunction(e->getType())) {
    vector<TypePtr> generics;
    if (auto t = CAST(expr->index, TupleExpr))
      for (auto &i : t->items)
        generics.push_back(transformType(i)->getType());
    else
      generics.push_back(transformType(expr->index)->getType());

    auto uf = [&](auto &f) {
      if (f->generics.size() != generics.size())
        error(expr, "inconsistent generic count");
      for (int i = 0; i < generics.size(); i++)
        if (f->generics[i].second->unify(generics[i]) < 0)
          error(e, "cannot unify {} and {}", *f->generics[i].second,
                *generics[i]);
    };
    // Instantiate the type
    // TODO: special cases (function, tuple, Kmer, Int, UInt)
    if (auto f = getFunction(e->getType()))
      uf(f);
    else if (auto g = getClass(e->getType()))
      uf(g);
    else
      assert(false);

    auto t = e->getType();
    resultExpr = N<TypeOfExpr>(move(e));
    resultExpr->markType();
    resultExpr->setType(t);
  } /* TODO: handle tuple access */ else {
    resultExpr = transform(
        N<CallExpr>(N<DotExpr>(move(e), "__getitem__"), expr->index->clone()));
  }
}

void TransformVisitor::visit(const CallExpr *expr) {
  // TODO: argument name resolution should come here!
  // TODO: handle the case when a member is of type function[...]

  ExprPtr e = nullptr;
  vector<CallExpr::Arg> args;
  // Intercept obj.foo() calls and transform obj.foo(...) to foo(obj, ...)
  if (auto d = CAST(expr->expr, DotExpr)) {
    auto dotlhs = transform(d->expr, true);
    if (!dotlhs->isType()) {
      // Find appropriate function!
      if (auto c = getClass(dotlhs->getType())) {
        if (auto m = ctx.findMethod(c->name, d->member)) {
          args.push_back({"self", move(dotlhs)});
          e = N<IdExpr>(m->name);
          e->setType(ctx.instantiate(getSrcInfo(), m, c->generics));
        } else {
          error(d, "{} has no method '{}'", *dotlhs->getType(), d->member);
        }
      } else if (!getUnbound(dotlhs->getType())) {
        error(d, "type {} has no methods", *d->expr->getType());
      }
    }
  }
  if (!e)
    e = transform(expr->expr, true);
  // Unify the call
  forceUnify(expr->expr, e->getType());
  // Handle other arguments, if any
  for (auto &i : expr->args)
    args.push_back({i.name, transform(i.value)});

  // If constructor, replace with appropriate calls
  if (e->isType()) {
    assert(getClass(e->getType()));
    string var = getTemporaryVar("typ");
    if (getClass(e->getType())->isRecord) {
      prepend(N<AssignStmt>(
          N<IdExpr>(var),
          N<CallExpr>(N<DotExpr>(e->clone(), "__init__"), move(args))));
    } else {
      prepend(N<AssignStmt>(N<IdExpr>(var),
                            N<CallExpr>(N<DotExpr>(e->clone(), "__new__"))));
      prepend(N<ExprStmt>(
          N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "__init__"), move(args))));
    }
    resultExpr = transform(N<IdExpr>(var));
  } else if (auto t = getFunction(e->getType())) {
    string torig = t->toString();
    for (int i = 0; i < args.size(); i++) {
      if (t->args[i].second->unify(args[i].value->getType()) < 0)
        error(expr, "cannot unify {} and {}", *t->args[i].second,
              *args[i].value->getType());
    }
    DBG("attempting to realize {} / {}", *t, t->canRealize());
    if (t->canRealize())
      t = realize(t).type;
    resultExpr = N<CallExpr>(move(e), move(args));
    resultExpr->setType(make_shared<LinkType>(t->ret));
    forceUnify(expr, resultExpr->getType());
  } else { // will be handled later on
    resultExpr = N<CallExpr>(move(e), move(args));
    resultExpr->setType(expr->getType() ? expr->getType()
                                        : ctx.addUnbound(getSrcInfo()));
  }
}

void TransformVisitor::visit(const DotExpr *expr) {
  // TODO: handle imports

  auto lhs = transform(expr->expr, true);
  TypePtr typ = nullptr;
  if (getUnbound(lhs->getType())) {
    typ = expr->getType() ? expr->getType() : ctx.addUnbound(getSrcInfo());
  } else if (auto c = getClass(lhs->getType())) {
    if (auto m = ctx.findMethod(c->name, expr->member)) {
      if (lhs->isType()) {
        resultExpr = N<IdExpr>(m->name);
        resultExpr->setType(ctx.instantiate(getSrcInfo(), m, c->generics));
        return;
      } else
        error(expr, "cannot handle partials yet");
      // TODO: for now, this method cannot handle obj.method expression
      // (CallExpr does that for obj.method() expressions).
    } else if (auto mm = ctx.findMember(c->name, expr->member)) {
      typ = ctx.instantiate(getSrcInfo(), mm, c->generics);
    } else {
      error(expr, "cannot find '{}' in {}", expr->member, *lhs->getType());
    }
  } else {
    error(expr, "cannot search for '{}' in {}", expr->member, *lhs->getType());
  }
  forceUnify(expr, typ);
  resultExpr = N<DotExpr>(move(lhs), expr->member);
  resultExpr->setType(typ);
}

// Transformation
void TransformVisitor::visit(const SliceExpr *expr) {
  string prefix;
  if (!expr->st && expr->ed)
    prefix = "l";
  else if (expr->st && !expr->ed)
    prefix = "r";
  else if (!expr->st && !expr->ed)
    prefix = "e";
  if (expr->step)
    prefix += "s";

  vector<ExprPtr> args;
  if (expr->st)
    args.push_back(expr->st->clone());
  if (expr->ed)
    args.push_back(expr->ed->clone());
  if (expr->step)
    args.push_back(expr->step->clone());
  if (!args.size())
    args.push_back(N<IntExpr>(0));
  resultExpr = transform(N<CallExpr>(N<IdExpr>(prefix + "slice"), move(args)));
}

void TransformVisitor::visit(const EllipsisExpr *expr) {
  resultExpr = N<EllipsisExpr>();
}

// Should get transformed by other functions
void TransformVisitor::visit(const TypeOfExpr *expr) {
  resultExpr = N<TypeOfExpr>(transform(expr->expr, true));
  resultExpr->markType();
  auto t = expr->expr->getType();
  forceUnify(expr, t);
  resultExpr->setType(t);
}

void TransformVisitor::visit(const PtrExpr *expr) {
  auto param = transform(expr->expr);
  auto t = ctx.instantiateGeneric(expr->getSrcInfo(), ctx.findInternal("ptr"),
                                  {param->getType()});
  resultExpr = N<PtrExpr>(move(param));
  forceUnify(expr, t);
  resultExpr->setType(t);
}

// Transformation
void TransformVisitor::visit(const LambdaExpr *expr) {
  CaptureVisitor cv(ctx);
  expr->expr->accept(cv);

  vector<Param> params;
  for (auto &s : expr->vars)
    params.push_back({s, nullptr, nullptr});
  for (auto &c : cv.captures)
    params.push_back({c, nullptr, nullptr});
  string fnVar = getTemporaryVar("anonFn");
  prepend(N<FunctionStmt>(fnVar, nullptr, vector<string>{}, move(params),
                          N<ReturnStmt>(expr->expr->clone()),
                          vector<string>{}));
  vector<CallExpr::Arg> args;
  for (int i = 0; i < expr->vars.size(); i++)
    args.push_back({"", N<EllipsisExpr>()});
  for (auto &c : cv.captures)
    args.push_back({"", N<IdExpr>(c)});
  resultExpr = transform(N<CallExpr>(N<IdExpr>(fnVar), move(args)));
}

// TODO
void TransformVisitor::visit(const YieldExpr *expr) {
  error(expr, "todo yieldexpr");
}

/*************************************************************************************/

void TransformVisitor::visit(const SuiteStmt *stmt) {
  vector<StmtPtr> r;
  for (auto &s : stmt->stmts)
    if (auto t = transform(s))
      r.push_back(move(t));
  resultStmt = N<SuiteStmt>(move(r));
}

void TransformVisitor::visit(const PassStmt *stmt) {
  resultStmt = N<PassStmt>();
}

void TransformVisitor::visit(const BreakStmt *stmt) {
  resultStmt = N<BreakStmt>();
}

void TransformVisitor::visit(const ContinueStmt *stmt) {
  resultStmt = N<ContinueStmt>();
}

void TransformVisitor::visit(const ExprStmt *stmt) {
  resultStmt = N<ExprStmt>(transform(stmt->expr));
}

StmtPtr TransformVisitor::addAssignment(const Expr *lhs, const Expr *rhs,
                                        const Expr *type, bool force) {
  if (auto l = dynamic_cast<const IndexExpr *>(lhs)) {
    vector<ExprPtr> args;
    args.push_back(l->index->clone());
    args.push_back(rhs->clone());
    return transform(Nx<ExprStmt>(
        lhs,
        Nx<CallExpr>(lhs, Nx<DotExpr>(lhs, l->expr->clone(), "__setitem__"),
                     move(args))));
  } else if (auto l = dynamic_cast<const DotExpr *>(lhs)) {
    // TODO Member Expr
    return Nx<AssignStmt>(lhs, transform(lhs), transform(rhs));
  } else if (auto l = dynamic_cast<const IdExpr *>(lhs)) {
    auto typExpr = transform(type);
    if (typExpr && !typExpr->isType())
      error(typExpr, "expected a type");
    TypePtr typ = typExpr ? typExpr->getType() : nullptr;
    auto s = Nx<AssignStmt>(lhs, Nx<IdExpr>(l, l->value), transform(rhs),
                            transform(type, true), false, force);
    if (typ && typ->unify(s->rhs->getType()) < 0)
      error(rhs, "type {} is not compatible with {}", *typ, *s->rhs->getType());
    if (auto t = ctx.find(l->value)) {
      if (t->getType()->unify(s->rhs->getType()) < 0)
        error(rhs, "type {} is not compatible with {}", *t->getType(),
              *s->rhs->getType());
      s->lhs->setType(t->getType());
    } else {
      ctx.add(l->value, s->rhs->getType(), s->rhs->isType());
      s->lhs->setType(s->rhs->getType());
    }
    return s;
  } else {
    error(lhs->getSrcInfo(), "invalid assignment");
    return nullptr;
  }
}

void TransformVisitor::processAssignment(const Expr *lhs, const Expr *rhs,
                                         vector<StmtPtr> &stmts, bool force) {
  vector<Expr *> lefts;
  if (auto l = dynamic_cast<const TupleExpr *>(lhs)) {
    for (auto &i : l->items)
      lefts.push_back(i.get());
  } else if (auto l = dynamic_cast<const ListExpr *>(lhs)) {
    for (auto &i : l->items)
      lefts.push_back(i.get());
  } else {
    stmts.push_back(addAssignment(lhs, rhs, nullptr, force));
    return;
  }
  if (!dynamic_cast<const IdExpr *>(rhs)) { // store any non-trivial expression
    auto var = getTemporaryVar("assign");
    auto newRhs = Nx<IdExpr>(rhs, var).release();
    stmts.push_back(addAssignment(newRhs, rhs, nullptr, force));
    rhs = newRhs;
  }
  UnpackExpr *unpack = nullptr;
  int st = 0;
  for (; st < lefts.size(); st++) {
    if (auto u = dynamic_cast<UnpackExpr *>(lefts[st])) {
      unpack = u;
      break;
    }
    processAssignment(
        lefts[st],
        Nx<IndexExpr>(rhs, rhs->clone(), Nx<IntExpr>(rhs, st)).release(), stmts,
        force);
  }
  if (unpack) {
    processAssignment(
        unpack->what.get(),
        Nx<IndexExpr>(
            rhs, rhs->clone(),
            Nx<SliceExpr>(rhs, Nx<IntExpr>(rhs, st),
                          lefts.size() == st + 1
                              ? nullptr
                              : Nx<IntExpr>(rhs, -lefts.size() + st + 1),
                          nullptr))
            .release(),
        stmts, force);
    st += 1;
    for (; st < lefts.size(); st++) {
      if (dynamic_cast<UnpackExpr *>(lefts[st]))
        error(lefts[st]->getSrcInfo(), "two unpack expressions in assignment");
      processAssignment(
          lefts[st],
          Nx<IndexExpr>(rhs, rhs->clone(), Nx<IntExpr>(rhs, -lefts.size() + st))
              .release(),
          stmts, force);
    }
  }
}

// Transformation
void TransformVisitor::visit(const AssignStmt *stmt) {
  vector<StmtPtr> stmts;
  if (stmt->type) {
    if (auto i = CAST(stmt->lhs, IdExpr))
      stmts.push_back(
          addAssignment(stmt->lhs.get(), stmt->rhs.get(), stmt->type.get()));
    else
      error(stmt, "only a single target can be annotated");
  } else {
    processAssignment(stmt->lhs.get(), stmt->rhs.get(), stmts);
  }
  resultStmt = stmts.size() == 1 ? move(stmts[0]) : N<SuiteStmt>(move(stmts));
}

// Transformation
void TransformVisitor::visit(const DelStmt *stmt) {
  if (auto expr = CAST(stmt->expr, IndexExpr)) {
    resultStmt = N<ExprStmt>(transform(N<CallExpr>(
        N<DotExpr>(expr->expr->clone(), "__delitem__"), expr->index->clone())));
  } else if (auto expr = CAST(stmt->expr, IdExpr)) {
    ctx.remove(expr->value);
    resultStmt = N<DelStmt>(transform(expr));
  } else {
    error(stmt, "this expression cannot be deleted");
  }
}

// Transformation
void TransformVisitor::visit(const PrintStmt *stmt) {
  resultStmt = N<PrintStmt>(conditionalMagic(stmt->expr, "str", "__str__"));
}

void TransformVisitor::visit(const ReturnStmt *stmt) {
  if (stmt->expr) {
    ctx.hasSetReturnType = true;
    auto e = transform(stmt->expr);
    if (ctx.returnType->unify(e->getType()) < 0)
      error(stmt, "incompatible return types: {} and {}", *e->getType(),
            *ctx.returnType);
    resultStmt = N<ReturnStmt>(move(e));
  } else {
    // if (ctx.returnType->unify(ctx.findInternal("void")) < 0)
    //   error(stmt, "incompatible return types: void and {}", *ctx.returnType);
    resultStmt = N<ReturnStmt>(nullptr);
  }
}

void TransformVisitor::visit(const YieldStmt *stmt) {
  ctx.hasSetReturnType = true;
  if (stmt->expr) {
    auto e = transform(stmt->expr);
    auto t = ctx.instantiateGeneric(
        e->getSrcInfo(), ctx.findInternal("generator"), {e->getType()});
    if (ctx.returnType->unify(t) < 0)
      error(stmt, "incompatible return types: {} and {}", *t, *ctx.returnType);
    resultStmt = N<YieldStmt>(move(e));
  } else {
    auto t = ctx.instantiateGeneric(stmt->getSrcInfo(),
                                    ctx.findInternal("generator"),
                                    {ctx.findInternal("void")});
    if (ctx.returnType->unify(ctx.findInternal("void")) < 0)
      error(stmt, "incompatible return types: void and {}", *ctx.returnType);
    resultStmt = N<YieldStmt>(nullptr);
  }
}

void TransformVisitor::visit(const AssertStmt *stmt) {
  resultStmt = N<AssertStmt>(transform(stmt->expr));
}

void TransformVisitor::visit(const WhileStmt *stmt) {
  resultStmt = N<WhileStmt>(makeBoolExpr(stmt->cond), transform(stmt->suite));
}

void TransformVisitor::visit(const ForStmt *stmt) {
  auto iter = conditionalMagic(stmt->iter, "generator", "__iter__");
  auto varType = ctx.addUnbound(stmt->var->getSrcInfo());
  if (!getUnbound(iter->getType())) {
    auto iterType = getClass(iter->getType());
    if (!iterType || iterType->name != "generator")
      error(iter, "not a generator");
    if (varType->unify(iterType->generics[0].second) < 0)
      error(stmt->iter, "cannot unify");
  }

  if (auto i = CAST(stmt->var, IdExpr)) {
    string varName = i->value;
    ctx.add(varName, varType);
    resultStmt =
        N<ForStmt>(transform(stmt->var), move(iter), transform(stmt->suite));
    ctx.remove(varName);
  } else {
    string varName = getTemporaryVar("for");
    ctx.add(varName, varType);
    vector<StmtPtr> stmts;
    auto var = N<IdExpr>(varName);
    processAssignment(stmt->var.get(), var.get(), stmts, true);
    stmts.push_back(stmt->suite->clone());
    resultStmt = N<ForStmt>(var->clone(), move(iter),
                            transform(N<SuiteStmt>(move(stmts))));
    ctx.remove(varName);
  }
}

void TransformVisitor::visit(const IfStmt *stmt) {
  vector<IfStmt::If> ifs;
  for (auto &i : stmt->ifs)
    ifs.push_back({makeBoolExpr(i.cond), transform(i.suite)});
  resultStmt = N<IfStmt>(move(ifs));
}

// TODO
void TransformVisitor::visit(const MatchStmt *stmt) {
  error(stmt, "todo match");
  // resultPattern = N<MatchStmt>(transform(stmt->what),
  // transform(stmt->patterns),
  //  transform(stmt->cases));
}

void TransformVisitor::visit(const ExtendStmt *stmt) {
  string name;
  vector<string> generics;
  vector<pair<int, TypePtr>> genericTypes;
  if (auto e = CAST(stmt->what, IndexExpr)) {
    if (auto i = CAST(e->expr, IdExpr)) {
      name = i->value;
      if (auto t = CAST(e->index, TupleExpr))
        for (auto &ti : t->items)
          if (auto s = CAST(ti, IdExpr))
            generics.push_back(s->value);
          else
            error(ti, "not a valid generic");
      else if (auto i = CAST(e->index, IdExpr))
        generics.push_back(i->value);
      else
        error(e->index, "not a valid generic");
    } else
      error(e, "not a valid type expression");
  } else if (auto i = CAST(stmt->what, IdExpr)) {
    name = i->value;
  } else {
    error(stmt->what, "not a valid type expression");
  }
  auto val = ctx.find(name);
  if (!val)
    error(stmt->what, "identifier '{}' not found", name);
  if (!val->isType())
    error(stmt->what, "not a type");
  auto type = val->getType();
  auto canonicalName = ctx.getCanonicalName(type->getSrcInfo());
  if (auto c = getClass(type)) {
    if (c->generics.size() != generics.size())
      error(stmt, "generics do not match");
    for (int i = 0; i < generics.size(); i++) {
      genericTypes.push_back(make_pair(
          c->generics[i].first,
          make_shared<LinkType>(LinkType::Generic, c->generics[i].first)));
      ctx.add(generics[i],
              make_shared<LinkType>(LinkType::Unbound, c->generics[i].first,
                                    ctx.level),
              true);
    }
  } else
    error(stmt, "cannot extend this");
  ctx.increaseLevel();
  ctx.bases.push_back(name);
  auto addMethod = [&](auto s) {
    if (auto f = dynamic_cast<FunctionStmt *>(s)) {
      transform(s);
      auto val = ctx.find(format("{}{}", ctx.getBase(), f->name));
      auto fval = getFunction(val->getType());
      assert(fval);
      fval->setImplicits(genericTypes);
      ctx.classes[canonicalName].methods[f->name] = fval;
    } else {
      error(s, "types can only contain functions");
    }
  };
  for (auto s : stmt->suite->getStatements())
    addMethod(s);
  ctx.decreaseLevel();
  for (auto &g : generics) {
    auto t = dynamic_pointer_cast<LinkType>(ctx.find(g)->getType());
    assert(t && getUnbound(t));
    t->kind = LinkType::Generic;
    ctx.remove(g);
  }
  ctx.bases.pop_back();
  resultStmt = nullptr;
}

// TODO
void TransformVisitor::visit(const ImportStmt *stmt) {
  error(stmt, "todo import");
  // resultPattern = N<ImportStmt>(stmt->from, stmt->what);
}

// Transformation
void TransformVisitor::visit(const ExternImportStmt *stmt) {
  if (stmt->lang == "c" && stmt->from) {
    vector<StmtPtr> stmts;
    // ptr = _dlsym(FROM, WHAT)
    stmts.push_back(N<AssignStmt>(
        N<IdExpr>("ptr"), N<CallExpr>(N<IdExpr>("_dlsym"), stmt->from->clone(),
                                      N<StringExpr>(stmt->name.first))));
    // f = function[ARGS](ptr)
    vector<ExprPtr> args;
    args.push_back(stmt->ret ? stmt->ret->clone() : N<IdExpr>("void"));
    for (auto &a : stmt->args)
      args.push_back(a.type->clone());
    stmts.push_back(N<AssignStmt>(
        N<IdExpr>("f"), N<CallExpr>(N<IndexExpr>(N<IdExpr>("function"),
                                                 N<TupleExpr>(move(args))),
                                    N<IdExpr>("ptr"))));
    bool isVoid = true;
    if (stmt->ret) {
      if (auto f = CAST(stmt->ret, IdExpr))
        isVoid = f->value == "void";
      else
        isVoid = false;
    }
    args.clear();
    for (int i = 0; i < stmt->args.size(); i++)
      args.push_back(N<IdExpr>(stmt->args[i].name != "" ? stmt->args[i].name
                                                        : format("$a{}", i)));
    // return f(args)
    auto call = N<CallExpr>(N<IdExpr>("f"), move(args));
    if (!isVoid)
      stmts.push_back(N<ReturnStmt>(move(call)));
    else
      stmts.push_back(N<ExprStmt>(move(call)));
    // def WHAT(args):
    vector<Param> params;
    for (int i = 0; i < stmt->args.size(); i++)
      params.push_back(
          {stmt->args[i].name != "" ? stmt->args[i].name : format("$a{}", i),
           stmt->args[i].type->clone()});
    resultStmt = transform(N<FunctionStmt>(
        stmt->name.second != "" ? stmt->name.second : stmt->name.first,
        stmt->ret->clone(), vector<string>(), move(params),
        N<SuiteStmt>(move(stmts)), vector<string>()));
  } else if (stmt->lang == "c") {
    vector<Param> args;
    for (auto &a : stmt->args)
      args.push_back({a.name, transformType(a.type), transform(a.deflt)});
    resultStmt =
        N<ExternImportStmt>(stmt->name, transform(stmt->from),
                            transformType(stmt->ret), move(args), stmt->lang);
  } else if (stmt->lang == "py") {
    vector<StmtPtr> stmts;
    string from = "";
    if (auto i = CAST(stmt->from, IdExpr))
      from = i->value;
    else
      error(stmt, "invalid pyimport query");
    auto call = N<CallExpr>( // _py_import(LIB)[WHAT].call (x.__to_py__)
        N<DotExpr>(N<IndexExpr>(N<CallExpr>(N<IdExpr>("_py_import"),
                                            N<StringExpr>(from)),
                                N<StringExpr>(stmt->name.first)),
                   "call"),
        N<CallExpr>(N<DotExpr>(N<IdExpr>("x"), "__to_py__")));
    bool isVoid = true;
    if (stmt->ret) {
      if (auto f = CAST(stmt->ret, IdExpr))
        isVoid = f->value == "void";
      else
        isVoid = false;
    }
    if (!isVoid) // return TYP.__from_py__(call)
      stmts.push_back(N<ReturnStmt>(N<CallExpr>(
          N<DotExpr>(stmt->ret->clone(), "__from_py__"), move(call))));
    else
      stmts.push_back(N<ExprStmt>(move(call)));
    vector<Param> params;
    params.push_back({"x", nullptr, nullptr});
    resultStmt = transform(N<FunctionStmt>(
        stmt->name.second != "" ? stmt->name.second : stmt->name.first,
        stmt->ret->clone(), vector<string>(), move(params),
        N<SuiteStmt>(move(stmts)), vector<string>{"pyhandle"}));
  } else {
    error(stmt, "language {} not yet supported", stmt->lang);
  }
}

void TransformVisitor::visit(const TryStmt *stmt) {
  vector<TryStmt::Catch> catches;
  for (auto &c : stmt->catches) {
    auto exc = transformType(c.exc);
    if (c.var != "")
      ctx.add(c.var, exc->getType());
    catches.push_back({c.var, move(exc), transform(c.suite)});
    ctx.remove(c.var);
  }
  resultStmt = N<TryStmt>(transform(stmt->suite), move(catches),
                          transform(stmt->finally));
}

void TransformVisitor::visit(const GlobalStmt *stmt) {
  // TODO: global stmts
  resultStmt = N<GlobalStmt>(stmt->var);
}

void TransformVisitor::visit(const ThrowStmt *stmt) {
  resultStmt = N<ThrowStmt>(transform(stmt->expr));
}

void TransformVisitor::visit(const FunctionStmt *stmt) {
  auto canonicalName = ctx.generateCanonicalName(
      stmt->getSrcInfo(), format("{}{}", ctx.getBase(), stmt->name));
  if (ctx.funcASTs.find(canonicalName) == ctx.funcASTs.end()) {
    vector<pair<int, TypePtr>> genericTypes;
    vector<pair<string, TypePtr>> argTypes;
    ctx.addBlock();
    for (auto &g : stmt->generics) {
      auto t =
          make_shared<LinkType>(LinkType::Unbound, ctx.unboundCount, ctx.level);
      ctx.add(g, t, true);
      genericTypes.push_back(make_pair(ctx.unboundCount, t));
      ctx.unboundCount++;
    }
    ctx.increaseLevel();
    vector<Param> args;
    for (auto &a : stmt->args) {
      auto t = transformType(a.type);
      argTypes.push_back(make_pair(
          a.name, a.type ? t->getType() : ctx.addUnbound(getSrcInfo(), false)));
      args.push_back({a.name, move(t), transform(a.deflt)});
    }
    auto ret = stmt->ret ? transformType(stmt->ret)->getType()
                         : ctx.addUnbound(getSrcInfo(), false);
    ctx.decreaseLevel();
    ctx.popBlock();

    auto type =
        make_shared<FuncType>(canonicalName, genericTypes, argTypes, ret);
    auto t = type->generalize(ctx.level);
    DBG("* [function] {} :- {}", canonicalName, *t);
    ctx.add(format("{}{}", ctx.getBase(), stmt->name), t);

    ctx.funcASTs[canonicalName] = make_pair(
        t, N<FunctionStmt>(canonicalName, nullptr, stmt->generics, move(args),
                           stmt->suite, stmt->attributes));
  }
  resultStmt = N<FunctionStmt>(canonicalName, nullptr, vector<string>(),
                               vector<Param>(), nullptr, stmt->attributes);
}

void TransformVisitor::visit(const ClassStmt *stmt) {
  auto canonicalName = ctx.generateCanonicalName(
      stmt->getSrcInfo(), format("{}{}", ctx.getBase(), stmt->name));
  resultStmt = N<ClassStmt>(stmt->isRecord, canonicalName, vector<string>(),
                            vector<Param>(), N<SuiteStmt>());
  if (ctx.classASTs.find(canonicalName) != ctx.classASTs.end())
    return;

  vector<pair<int, TypePtr>> genericTypes;
  vector<string> generics;
  for (auto &g : stmt->generics) {
    auto t = make_shared<LinkType>(LinkType::Generic, ctx.unboundCount);
    genericTypes.push_back(make_pair(ctx.unboundCount, t));
    auto tp =
        make_shared<LinkType>(LinkType::Unbound, ctx.unboundCount, ctx.level);
    ctx.add(g, tp, true);
    ctx.unboundCount++;
  }
  auto addMethod = [&](auto s) {
    if (auto f = dynamic_cast<FunctionStmt *>(s)) {
      transform(s);
      auto val = ctx.find(format("{}{}", ctx.getBase(), f->name));
      auto fval = getFunction(val->getType());
      assert(fval);
      fval->setImplicits(genericTypes);
      ctx.classes[canonicalName].methods[f->name] = fval;
      DBG("* [class] {} :: {} :- {}", canonicalName, f->name, *fval);
    } else {
      error(s, "types can only contain functions");
    }
  };
  // Classes are handled differently as they can contain recursive
  // references
  if (!stmt->isRecord) {
    auto ct = make_shared<ClassType>(canonicalName, false, genericTypes,
                                     vector<pair<string, TypePtr>>());
    ctx.add(format("{}{}", ctx.getBase(), stmt->name), ct, true);
    ctx.classASTs[canonicalName] = make_pair(ct, nullptr); // TODO: fix
    DBG("* [class] {} :- {}", canonicalName, *ct);

    ctx.increaseLevel();
    vector<string> strArgs;
    for (auto &a : stmt->args) {
      assert(a.type);
      strArgs.push_back(
          format("{}: {}", a.name, ast::FormatVisitor::format(ctx, a.type)));
      ctx.classes[canonicalName].members[a.name] =
          transformType(a.type)->getType()->generalize(ctx.level);
      // DBG("* [class] [member.{}] :- {}", a.name,
      // *ctx.classes[canonicalName].members[a.name]);
    }
    ctx.bases.push_back(stmt->name);

    if (!ctx.hasFlag("internal")) {
      auto codeType =
          format("{}{}", stmt->name,
                 stmt->generics.size()
                     ? format("[{}]", fmt::join(stmt->generics, ", "))
                     : "");
      auto code =
          format("@internal\ndef __new__() -> {0}: pass\n"
                 "@internal\ndef __bool__(self: {0}) -> bool: pass\n"
                 "@internal\ndef __pickle__(self: {0}, dest: ptr[byte]) -> "
                 "void: pass\n"
                 "@internal\ndef __unpickle__(src: ptr[byte]) -> {0}: pass\n"
                 "@internal\ndef __raw__(self: {0}) -> ptr[byte]: pass\n",
                 codeType);
      if (stmt->args.size())
        code += format("@internal\ndef __init__(self: {}, {}) -> void: pass\n",
                       codeType, fmt::join(strArgs, ", "));
      DBG("{}", code);
      auto methodNew =
          parse_code(ctx.filename, code, stmt->getSrcInfo().line, 100000);
      for (auto s : methodNew->getStatements())
        addMethod(s);
    }
    for (auto s : stmt->suite->getStatements())
      addMethod(s);
    ctx.decreaseLevel();
    ctx.bases.pop_back();
  } else {
    vector<pair<string, TypePtr>> argTypes;
    vector<string> strArgs;
    string mainType;
    for (auto &a : stmt->args) {
      assert(a.type);
      auto s = ast::FormatVisitor::format(ctx, a.type);
      strArgs.push_back(format("{}: {}", a.name, s));
      if (!mainType.size())
        mainType = s;

      auto t = transformType(a.type)->getType();
      argTypes.push_back({a.name, t});
      ctx.classes[canonicalName].members[a.name] = t;
    }
    if (!mainType.size())
      mainType = "void";
    auto ct =
        make_shared<ClassType>(canonicalName, true, genericTypes, argTypes);
    ctx.add(format("{}{}", ctx.getBase(), stmt->name), ct, true);
    ctx.classASTs[canonicalName] = make_pair(ct, nullptr); // TODO: fix
    ctx.bases.push_back(stmt->name);
    if (!ctx.hasFlag("internal")) {
      auto codeType =
          format("{}{}", stmt->name,
                 stmt->generics.size()
                     ? format("[{}]", fmt::join(stmt->generics, ", "))
                     : "");
      auto code = format(
          "@internal\ndef __init__({1}) -> {0}: pass\n"
          "@internal\ndef __str__(self: {0}) -> str: pass\n"
          "@internal\ndef __getitem__(self: {0}, idx: int) -> {2}: pass\n"
          "@internal\ndef __iter__(self: {0}) -> generator[{2}]: pass\n"
          "@internal\ndef __len__(self: {0}) -> int: pass\n"
          "@internal\ndef __eq__(self: {0}, other: {0}) -> bool: pass\n"
          "@internal\ndef __ne__(self: {0}, other: {0}) -> bool: pass\n"
          "@internal\ndef __lt__(self: {0}, other: {0}) -> bool: pass\n"
          "@internal\ndef __gt__(self: {0}, other: {0}) -> bool: pass\n"
          "@internal\ndef __le__(self: {0}, other: {0}) -> bool: pass\n"
          "@internal\ndef __ge__(self: {0}, other: {0}) -> bool: pass\n"
          "@internal\ndef __hash__(self: {0}) -> int: pass\n"
          "@internal\ndef __contains__(self: {0}, what: {2}) -> bool: pass\n"
          "@internal\ndef __pickle__(self: {0}, dest: ptr[byte]) -> void: "
          "pass\n"
          "@internal\ndef __unpickle__(src: ptr[byte]) -> {0}: pass\n"
          "@internal\ndef __to_py__(self: {0}) -> ptr[byte]: pass\n"
          "@internal\ndef __from_py__(src: ptr[byte]) -> {0}: pass\n",
          codeType, fmt::join(strArgs, ", "), mainType);
      auto methodNew =
          parse_code(ctx.filename, code, stmt->getSrcInfo().line, 100000);
      for (auto s : methodNew->getStatements())
        addMethod(s);
    }
    for (auto s : stmt->suite->getStatements())
      addMethod(s);
    ctx.bases.pop_back();
  }

  for (auto &g : stmt->generics) {
    // Generalize in place
    auto t = dynamic_pointer_cast<LinkType>(ctx.find(g)->getType());
    assert(t && t->kind == LinkType::Unbound);
    t->kind = LinkType::Generic;
    ctx.remove(g);
  }
}

// TODO
void TransformVisitor::visit(const DeclareStmt *stmt) {
  error(stmt, "todo declare");
}

// Transformation
void TransformVisitor::visit(const AssignEqStmt *stmt) {
  resultStmt = transform(N<AssignStmt>(
      stmt->lhs->clone(),
      N<BinaryExpr>(stmt->lhs->clone(), stmt->op, stmt->rhs->clone(), true),
      nullptr, true));
}

// Transformation
void TransformVisitor::visit(const YieldFromStmt *stmt) {
  auto var = getTemporaryVar("yield");
  resultStmt = transform(N<ForStmt>(N<IdExpr>(var), stmt->expr->clone(),
                                    N<YieldStmt>(N<IdExpr>(var))));
}

// Transformation
void TransformVisitor::visit(const WithStmt *stmt) {
  if (!stmt->items.size())
    error(stmt, "malformed with statement");
  vector<StmtPtr> content;
  for (int i = stmt->items.size() - 1; i >= 0; i--) {
    vector<StmtPtr> internals;
    string var = stmt->vars[i] == "" ? getTemporaryVar("with") : stmt->vars[i];
    internals.push_back(N<AssignStmt>(N<IdExpr>(var), stmt->items[i]->clone()));
    internals.push_back(
        N<ExprStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "__enter__"))));
    internals.push_back(N<TryStmt>(
        content.size() ? N<SuiteStmt>(move(content)) : stmt->suite->clone(),
        vector<TryStmt::Catch>{},
        N<SuiteStmt>(
            N<ExprStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "__exit__"))))));
    content = move(internals);
  }
  resultStmt =
      transform(N<IfStmt>(N<BoolExpr>(true), N<SuiteStmt>(move(content))));
}

// Transformation
void TransformVisitor::visit(const PyDefStmt *stmt) {
  // _py_exec(""" str """)
  vector<string> args;
  for (auto &a : stmt->args)
    args.push_back(a.name);
  string code = format("def {}({}):\n{}\n", stmt->name, fmt::join(args, ", "),
                       stmt->code);
  resultStmt = transform(N<SuiteStmt>(
      N<ExprStmt>(N<CallExpr>(N<IdExpr>("_py_exec"), N<StringExpr>(code))),
      // from __main__ pyimport foo () -> ret
      N<ExternImportStmt>(make_pair(stmt->name, ""), N<IdExpr>("__main__"),
                          stmt->ret->clone(), vector<Param>(), "py")));
}

void TransformVisitor::visit(const StarPattern *pat) {
  resultPattern = N<StarPattern>();
}

void TransformVisitor::visit(const IntPattern *pat) {
  resultPattern = N<IntPattern>(pat->value);
}

void TransformVisitor::visit(const BoolPattern *pat) {
  resultPattern = N<BoolPattern>(pat->value);
}

void TransformVisitor::visit(const StrPattern *pat) {
  resultPattern = N<StrPattern>(pat->value);
}

void TransformVisitor::visit(const SeqPattern *pat) {
  resultPattern = N<SeqPattern>(pat->value);
}

void TransformVisitor::visit(const RangePattern *pat) {
  resultPattern = N<RangePattern>(pat->start, pat->end);
}

void TransformVisitor::visit(const TuplePattern *pat) {
  resultPattern = N<TuplePattern>(transform(pat->patterns));
}

void TransformVisitor::visit(const ListPattern *pat) {
  resultPattern = N<ListPattern>(transform(pat->patterns));
}

void TransformVisitor::visit(const OrPattern *pat) {
  resultPattern = N<OrPattern>(transform(pat->patterns));
}

void TransformVisitor::visit(const WildcardPattern *pat) {
  resultPattern = N<WildcardPattern>(pat->var);
}

void TransformVisitor::visit(const GuardedPattern *pat) {
  resultPattern =
      N<GuardedPattern>(transform(pat->pattern), transform(pat->cond));
}

void TransformVisitor::visit(const BoundPattern *pat) {
  resultPattern = N<BoundPattern>(pat->var, transform(pat->pattern));
}

} // namespace ast
} // namespace seq
