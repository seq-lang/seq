/**
 * TODO here:
 * - Finish remaining statements
 * - Add bool transformation
 * - Handle __iop__/__rop__ magics
 * - Fix remaining transformation
 * - Add SrcInfo to types to support better error messages
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
#include "parser/common.h"
#include "parser/context.h"
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
using std::stack;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::unordered_set;
using std::vector;

int __level__ = 0;

// TODO: get rid of this macro
#define RETURN(T, ...)                                                         \
  (this->result =                                                              \
       fwdSrcInfo(make_unique<T>(__VA_ARGS__), expr->getSrcInfo()));           \
  return
// TODO: use hack from common.h to glob the comma from __VA_ARGS__
#define ERROR(s, ...) error(s->getSrcInfo(), __VA_ARGS__)
#define CAST(s, T) dynamic_cast<T *>(s.get())

#define TO_BOOL(e)                                                             \
  (Nx<CallExpr>(e.get(), Nx<IdExpr>(e.get(), "bool"), e->clone()))

namespace seq {
namespace ast {

TypeContext::TypeContext(const std::string &filename)
    : filename(filename), module(""), prefix(""), level(0), unboundCount(0),
      returnType(nullptr), hasSetReturnType(false) {
  // set up internals
  stack.push(vector<string>());
  vector<string> podTypes = {"void", "bool", "int", "float",
                             "byte", "str",  "seq"};
  for (auto &t : podTypes) {
    internals[t] = make_shared<ClassType>(t, t, vector<pair<int, TypePtr>>());
    moduleNames[t] = 1;
  }
  /// TODO: array, __array__, ptr, generator, tuple etc
  /// UInt / Kmer
}

TypePtr TypeContext::find(const std::string &name, bool *isType) const {
  auto t = VTable<Type>::find(name);
  if (!t) {
    auto it = internals.find(name);
    if (it != internals.end()) {
      if (isType)
        *isType = true;
      return it->second;
    } else {
      return nullptr;
    }
  } else {
    if (isType) {
      auto it = this->isType.find(name);
      assert(it != this->isType.end());
      *isType = it->second.top();
    }
    return t;
  }
}

TypePtr TypeContext::findInternal(const std::string &name) const {
  auto it = internals.find(name);
  if (it != internals.end()) {
    return it->second;
  }
  return nullptr;
}

void TypeContext::increaseLevel() { level++; }

void TypeContext::decreaseLevel() { level--; }

string TypeContext::getCanonicalName(const seq::SrcInfo &info) {
  auto it = canonicalNames.find(info);
  if (it != canonicalNames.end()) {
    return it->second;
  }
  return "";
}

string TypeContext::getCanonicalName(const std::string &name,
                                     const seq::SrcInfo &info) {
  auto it = canonicalNames.find(info);
  if (it != canonicalNames.end())
    return it->second;

  auto &num = moduleNames[name];
  auto newName = (module == "" ? "" : module + ".");
  newName += name;
  newName += (num ? format(".{}", num) : "");
  num++;
  canonicalNames[info] = newName;
  return newName;
}

shared_ptr<LinkType> TypeContext::addUnbound(const seq::SrcInfo &srcInfo,
                                             bool setActive) {
  auto t = make_shared<LinkType>(LinkType::Unbound, unboundCount, level);
  t->setSrcInfo(srcInfo);
  if (setActive) {
    activeUnbounds.insert(t);
    DBG("UNBOUND {} ADDED # {} ", t, srcInfo.line);
  }
  unboundCount++;
  return t;
}

TypePtr TypeContext::instantiate(const seq::SrcInfo &srcInfo, TypePtr type) {
  return instantiate(srcInfo, type, vector<pair<int, TypePtr>>());
}

TypePtr TypeContext::instantiate(const seq::SrcInfo &srcInfo, TypePtr type,
                                 const vector<pair<int, TypePtr>> &generics) {
  std::unordered_map<int, TypePtr> cache;
  for (auto &g : generics) {
    cache[g.first] = g.second;
  }
  auto t = type->instantiate(level, unboundCount, cache);
  for (auto &i : cache) {
    if (i.second->isUnbound()) {
      i.second->setSrcInfo(srcInfo);
      if (activeUnbounds.find(i.second) == activeUnbounds.end()) {
        DBG("UNBOUND {} ADDED # {} ",
            dynamic_pointer_cast<LinkType>(i.second)->id, srcInfo.line);
        activeUnbounds.insert(i.second);
      }
    }
  }
  return t;
}

shared_ptr<FuncType> TypeContext::findMethod(shared_ptr<ClassType> type,
                                             const string &method) {
  auto m = classMethods.find(type->getCanonicalName());
  if (m != classMethods.end()) {
    auto t = m->second.find(method);
    if (t != m->second.end()) {
      return t->second;
    }
  }
  return nullptr;
}

TypePtr TypeContext::findMember(shared_ptr<ClassType> type,
                                const string &member) {
  auto m = classMembers.find(type->getCanonicalName());
  if (m != classMembers.end()) {
    auto t = m->second.find(member);
    if (t != m->second.end()) {
      return t->second;
    }
  }
  return nullptr;
}

vector<pair<string, const FunctionStmt *>>
TypeContext::getRealizations(const FunctionStmt *stmt) {
  vector<pair<string, const FunctionStmt *>> result;
  auto it = canonicalNames.find(stmt->getSrcInfo());
  if (it != canonicalNames.end()) {
    for (auto &i : funcRealizations[it->second]) {
      result.push_back({i.first, i.second.second.get()});
    }
  }
  return result;
}

TransformExprVisitor::TransformExprVisitor(TypeContext &ctx,
                                           TransformStmtVisitor &sv)
    : ctx(ctx), stmtVisitor(sv) {}

ExprPtr TransformExprVisitor::transform(const Expr *expr) {
  TransformExprVisitor v(ctx, stmtVisitor);
  v.setSrcInfo(expr->getSrcInfo());
  DBG("[ {} :- {} # {}", *expr, expr->getType() ? expr->getType()->str() : "-",
      expr->getSrcInfo().line);
  __level__++;
  expr->accept(v);
  __level__--;
  DBG("  {} :- {} ]", *v.result,
      v.result->getType() ? v.result->getType()->str() : "-");
  return move(v.result);
}

void TransformExprVisitor::visit(const NoneExpr *expr) {
  result = expr->clone();
  if (!result->getType())
    result->setType(ctx.addUnbound(getSrcInfo()));
}

void TransformExprVisitor::visit(const BoolExpr *expr) {
  result = expr->clone();
  if (!result->getType())
    result->setType(T<LinkType>(ctx.findInternal("bool")));
}

void TransformExprVisitor::visit(const IntExpr *expr) {
  result = expr->clone();
  if (!result->getType())
    result->setType(T<LinkType>(ctx.findInternal("int")));
}

void TransformExprVisitor::visit(const FloatExpr *expr) {
  result = expr->clone();
  if (!result->getType())
    result->setType(T<LinkType>(ctx.findInternal("float")));
}

void TransformExprVisitor::visit(const StringExpr *expr) {
  result = expr->clone();
  if (!result->getType())
    result->setType(T<LinkType>(ctx.findInternal("str")));
}

// Transformed
void TransformExprVisitor::visit(const FStringExpr *expr) {
  int braceCount = 0, braceStart = 0;
  vector<ExprPtr> items;
  for (int i = 0; i < expr->value.size(); i++) {
    if (expr->value[i] == '{') {
      if (braceStart < i) {
        items.push_back(
            N<StringExpr>(expr->value.substr(braceStart, i - braceStart)));
      }
      if (!braceCount) {
        braceStart = i + 1;
      }
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
  if (braceCount) {
    ERROR(expr, "f-string braces not balanced");
  }
  if (braceStart != expr->value.size()) {
    items.push_back(N<StringExpr>(
        expr->value.substr(braceStart, expr->value.size() - braceStart)));
  }
  result = transform(N<CallExpr>(N<DotExpr>(N<IdExpr>("str"), "cat"),
                                 N<ListExpr>(move(items))));
}

// Transformed
void TransformExprVisitor::visit(const KmerExpr *expr) {
  result = transform(N<CallExpr>(
      N<IndexExpr>(N<IdExpr>("Kmer"), N<IntExpr>(expr->value.size())),
      N<SeqExpr>(expr->value)));
}

// Transformed
void TransformExprVisitor::visit(const SeqExpr *expr) {
  if (expr->prefix == "p") {
    result =
        transform(N<CallExpr>(N<IdExpr>("pseq"), N<StringExpr>(expr->value)));
  } else if (expr->prefix == "s") {
    result =
        transform(N<CallExpr>(N<IdExpr>("seq"), N<StringExpr>(expr->value)));
  } else {
    ERROR(expr, "invalid seq prefix '{}'", expr->prefix);
  }
}

void TransformExprVisitor::visit(const IdExpr *expr) {
  result = expr->clone();
  if (!expr->getType()) {
    bool isType = false;
    auto type = ctx.find(expr->value, &isType);
    if (!type)
      ERROR(expr, "identifier '{}' not found", expr->value);
    if (isType)
      result->markType();
    result->setType(ctx.instantiate(getSrcInfo(), type));
  }
}

// Transformed
void TransformExprVisitor::visit(const UnpackExpr *expr) {
  result = transform(N<CallExpr>(N<IdExpr>("list"), expr->what->clone()));
}

void TransformExprVisitor::visit(const TupleExpr *expr) {
  auto e = N<TupleExpr>(transform(expr->items));
  auto rt = T<RecordType>("", "", vector<pair<string, TypePtr>>());
  for (auto &i : e->items)
    rt->args.push_back({"", i->getType()});
  auto t = T<LinkType>(rt);
  if (expr->getType() && expr->getType()->unify(t) < 0)
    ERROR(expr, "cannot unify {} and {}", *expr->getType(), *t);
  e->setType(t);
  result = move(e);
}

// Transformed
void TransformExprVisitor::visit(const ListExpr *expr) {
  string listVar = getTemporaryVar("list");
  stmtVisitor.prepend(N<AssignStmt>(
      N<IdExpr>(listVar),
      N<CallExpr>(N<IdExpr>("list"), N<IntExpr>(expr->items.size()))));
  for (int i = 0; i < expr->items.size(); i++) {
    stmtVisitor.prepend(N<ExprStmt>(N<CallExpr>(
        N<DotExpr>(N<IdExpr>(listVar), "append"), expr->items[i]->clone())));
  }
  result = transform(N<IdExpr>(listVar));
}

// Transformed
void TransformExprVisitor::visit(const SetExpr *expr) {
  string setVar = getTemporaryVar("set");
  stmtVisitor.prepend(
      N<AssignStmt>(N<IdExpr>(setVar), N<CallExpr>(N<IdExpr>("set"))));
  for (int i = 0; i < expr->items.size(); i++) {
    stmtVisitor.prepend(N<ExprStmt>(N<CallExpr>(
        N<DotExpr>(N<IdExpr>(setVar), "add"), expr->items[i]->clone())));
  }
  result = transform(N<IdExpr>(setVar));
}

// Transformed
void TransformExprVisitor::visit(const DictExpr *expr) {
  string dictVar = getTemporaryVar("dict");
  stmtVisitor.prepend(
      N<AssignStmt>(N<IdExpr>(dictVar), N<CallExpr>(N<IdExpr>("dict"))));
  for (int i = 0; i < expr->items.size(); i++) {
    vector<ExprPtr> args;
    args.push_back(transform(expr->items[i].key));
    args.push_back(transform(expr->items[i].value));
    stmtVisitor.prepend(N<ExprStmt>(N<CallExpr>(
        N<DotExpr>(N<IdExpr>(dictVar), "__setitem__"), move(args))));
  }
  result = transform(N<IdExpr>(dictVar));
}

// Transformed
// TODO
void TransformExprVisitor::visit(const GeneratorExpr *expr) {
  ERROR(expr, "to be done later");
  vector<GeneratorExpr::Body> loops;
  for (auto &l : expr->loops) {
    loops.push_back({l.vars, transform(l.gen), transform(l.conds)});
  }
  RETURN(GeneratorExpr, expr->kind, transform(expr->expr), move(loops));
  /* TODO transform: T = list[T]() for_1: cond_1: for_2: cond_2: expr */
}

// Transformed
// TODO
void TransformExprVisitor::visit(const DictGeneratorExpr *expr) {
  ERROR(expr, "to be done later");
  vector<GeneratorExpr::Body> loops;
  for (auto &l : expr->loops) {
    loops.push_back({l.vars, transform(l.gen), transform(l.conds)});
  }
  RETURN(DictGeneratorExpr, transform(expr->key), transform(expr->expr),
         move(loops));
}

void TransformExprVisitor::visit(const IfExpr *expr) {
  auto e = N<IfExpr>(transform(TO_BOOL(expr->cond)), transform(expr->eif),
                     transform(expr->eelse));
  if (e->eif->getType()->unify(e->eelse->getType()) < 0)
    ERROR(expr, "type {} is not compatible with {}", e->eif->getType()->str(),
          e->eelse->getType()->str());
  auto t = e->eif->getType();
  if (expr->getType() && expr->getType()->unify(t) < 0)
    ERROR(expr, "cannot unify {} and {}", *expr->getType(), *t);
  e->setType(t);
  result = move(e);
}

// Transformed
void TransformExprVisitor::visit(const UnaryExpr *expr) {
  if (expr->op == "!") { // Special case
    auto e = N<UnaryExpr>(expr->op, transform(TO_BOOL(expr->expr)));
    auto t = T<LinkType>(ctx.findInternal("bool"));
    if (expr->getType() && expr->getType()->unify(t) < 0)
      ERROR(expr, "cannot unify {} and {}", *expr->getType(), *t);
    e->setType(t);
    result = move(e);
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
    ERROR(expr, "invalid unary operator '{}'", expr->op);
  magic = format("__{}__", magic);
  result = transform(N<CallExpr>(N<DotExpr>(expr->expr->clone(), magic)));
}

// Transformed
void TransformExprVisitor::visit(const BinaryExpr *expr) {
  if (expr->op == "&&" || expr->op == "||") { // Special case
    auto e = N<BinaryExpr>(transform(TO_BOOL(expr->lexpr)), expr->op,
                           transform(TO_BOOL(expr->rexpr)));
    auto t = T<LinkType>(ctx.findInternal("bool"));
    if (expr->getType() && expr->getType()->unify(t) < 0)
      ERROR(expr, "cannot unify {} and {}", *expr->getType(), *t);
    e->setType(t);
    result = move(e);
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
    ERROR(expr, "invalid binary operator '{}'", expr->op);
  // TODO: handle iop/rop
  magic = format("__{}__", magic);
  result = transform(N<CallExpr>(N<DotExpr>(expr->lexpr->clone(), magic),
                                 expr->rexpr->clone()));
}

// TODO
void TransformExprVisitor::visit(const PipeExpr *expr) {
  ERROR(expr, "to be done later");
  vector<PipeExpr::Pipe> items;
  for (auto &l : expr->items) {
    items.push_back({l.op, transform(l.expr)});
  }
  RETURN(PipeExpr, move(items));
}

void TransformExprVisitor::visit(const IndexExpr *expr) {
  // If this is a type or function realization
  // (e.g. dict[type1, type2]), handle it separately
  auto e = transform(expr->expr);
  if (e->isType() || e->getType()->getFunction()) {
    vector<TypePtr> generics;
    // TupleExpr* is used if we have more than one type
    if (auto t = CAST(expr->index, TupleExpr)) {
      for (auto &i : t->items) {
        auto it = transform(i);
        if (it->isType())
          generics.push_back(it->getType());
        else
          ERROR(it, "{} is not a type", *it);
      }
    } else {
      auto it = transform(expr->index);
      if (it->isType())
        generics.push_back(it->getType());
      else
        ERROR(it, "{} is not a type", *it);
    }
    // Instantiate the type
    if (auto f = e->getType()->getFunction()) {
      if (f->generics.size() != generics.size())
        ERROR(expr, "inconsistent generic count");
      for (int i = 0; i < generics.size(); i++)
        if (f->generics[i].second->unify(generics[i]) < 0)
          ERROR(e, "cannot unify {} and {}", *f->generics[i].second,
                *generics[i]);
    } else if (auto g = e->getType()->getClass()) {
      if (g->generics.size() != generics.size())
        ERROR(expr, "inconsistent generic count");
      for (int i = 0; i < generics.size(); i++)
        if (g->generics[i].second->unify(generics[i]) < 0)
          ERROR(e, "cannot unify {} and {}", *g->generics[i].second,
                *generics[i]);
    } else { // Cannot realize it at this time...
             // Should be impossible, but you never know...
      ERROR(expr, "cannot realize unknown type");
    }
    auto t = e->getType();
    // TODO: add class type realization at this stage
    // TODO: cast to fully qualified and realized IdExpr for easier access
    // if (t->canRealize()) {
    //   throw "";
    //   // stmtVisitor.realize(t);
    //   // result = N<IdExpr>(expr, t->getRealizedName());
    // } else {
    result = N<TypeOfExpr>(move(e)); // will be handled later
    // }
    result->markType();
    result->setType(t);
  }
  /* TODO: handle tuple access */
  else {
    result = transform(
        N<CallExpr>(N<DotExpr>(move(e), "__getitem__"), expr->index->clone()));
  }
}

void TransformExprVisitor::visit(const CallExpr *expr) {
  // TODO: argument name resolution should come here!
  // TODO: handle the case when a member is of type function[...]

  auto e = transform(expr->expr);

  vector<CallExpr::Arg> args;
  // Intercept obj.method() calls here for now to avoid partial types
  if (auto d = CAST(e, DotExpr)) {
    // Transform obj.method(...) to method_fn(obj, ...)
    if (!d->expr->isType()) {
      // Find appropriate function!
      if (auto c = d->expr->getType()->getClass()) {
        if (auto m = ctx.findMethod(c, d->member)) {
          args.push_back({"self", move(d->expr)});
          e = N<IdExpr>(m->getCanonicalName());
          e->setType(ctx.instantiate(getSrcInfo(), m, c->generics));
        } else {
          ERROR(d, "{} has no method '{}'", *d->expr->getType(), d->member);
        }
      } else if (d->expr->getType()->isUnbound()) {
        // Just leave it as-is;
        // it will be handled by the subsequent typecheck iterations.
        assert(e->getType()->isUnbound());
      } else {
        ERROR(d, "type {} has no methods", *d->expr->getType());
      }
    }
  }
  // Unify the call
  if (expr->expr->getType() && expr->expr->getType()->unify(e->getType()) < 0) {
    ERROR(expr->expr, "cannot unify {} and {}", *expr->expr->getType(),
          *e->getType());
  }
  // Handle other arguments, if any
  for (auto &i : expr->args)
    args.push_back({i.name, transform(i.value)});

  // If constructor, replace with appropriate calls
  if (e->isType()) {
    if (!e->getType()->getClass()) {
      ERROR(e, "cannot call non-type");
    }
    // DBG("[call] transform {} to __new__", *e->getType());
    // string name = e->getType()->getClass()->getCanonicalName();
    string var = getTemporaryVar("typ");
    stmtVisitor.prepend(N<AssignStmt>(
        N<IdExpr>(var), N<CallExpr>(N<DotExpr>(e->clone(), "__new__"))));
    stmtVisitor.prepend(N<ExprStmt>(
        N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "__init__"), move(args))));
    result = transform(N<IdExpr>(var));
  } else if (auto t = e->getType()->getFunction()) {
    // DBG("[call] [function] :- {}", *e->getType());
    string torig = t->str(), params = "";
    for (int i = 0; i < args.size(); i++) {
      params += args[i].value->getType()->str() + ",";
      if (t->args[i].second->unify(args[i].value->getType()) < 0) {
        ERROR(expr, "cannot unify {} and {}", *t->args[i].second,
              *args[i].value->getType());
      }
    }
    // Realize the function if that is possible
    if (t->canRealize()) {
      stmtVisitor.realize(t);
    }
    // DBG("[call] {} ({} + {}) :- {}", t->getCanonicalName(), torig, params,
    // *t);
    result = N<CallExpr>(move(e), move(args));
    result->setType(make_shared<LinkType>(t->ret));
    if (expr->getType() && expr->getType()->unify(result->getType()) < 0)
      ERROR(expr, "cannot unify {} and {}", *expr->getType(),
            *result->getType());
  } else { // will be handled later on
    result = N<CallExpr>(move(e), move(args));
    result->setType(expr->getType() ? expr->getType()
                                    : ctx.addUnbound(getSrcInfo()));
  }
}

void TransformExprVisitor::visit(const DotExpr *expr) {
  // TODO: handle imports

  auto lhs = transform(expr->expr);
  TypePtr typ = nullptr;
  if (lhs->getType()->isUnbound()) {
    typ = expr->getType() ? expr->getType() : ctx.addUnbound(getSrcInfo());
  } else if (auto c = lhs->getType()->getClass()) {
    // DBG("?? {} {} {} {}", *expr, *lhs, lhs->isType(), *lhs->getType());
    if (auto m = ctx.findMethod(c, expr->member)) {
      if (lhs->isType()) {
        typ = ctx.instantiate(getSrcInfo(), m, c->generics);
        // DBG("[dot] {} . {} :- {}  # t_raw: {}", *lhs->getType(),
        // expr->member,
        // *typ, *m);
      } else {
        // TODO: for now, this method cannot handle obj.method expression
        // (CallExpr does that for obj.method() expressions). Should be fixed
        // later...
        // DBG("[dot] {} . {} :- NULL", *lhs->getType(), expr->member);
      }
    } else if (auto m = ctx.findMember(c, expr->member)) { // is member
      typ = ctx.instantiate(getSrcInfo(), m, c->generics);
      // DBG("[dot] {} . {} :- {}  # m_raw: {}", *lhs->getType(), expr->member,
      // *typ, *m);
    } else {
      ERROR(expr, "cannot find '{}' in {}", expr->member, *lhs->getType());
    }
  } else {
    ERROR(expr, "cannot search for '{}' in {}", expr->member, *lhs->getType());
  }
  if (expr->getType() && typ)
    // DBG("[dot] UNIFY {} & {}", *typ, expr->getType());
    if (expr->getType() && typ && expr->getType()->unify(typ) < 0)
      ERROR(expr, "cannot unify {} and {}", *expr->getType(), *typ);
  result = N<DotExpr>(move(lhs), expr->member);
  result->setType(typ);
}

// Transformation
void TransformExprVisitor::visit(const SliceExpr *expr) {
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

  result = transform(N<CallExpr>(N<IdExpr>(prefix + "slice"), move(args)));
}

void TransformExprVisitor::visit(const EllipsisExpr *expr) {
  result = N<EllipsisExpr>();
}

// Should get transformed by other functions
void TransformExprVisitor::visit(const TypeOfExpr *expr) {
  result = N<TypeOfExpr>(transform(expr->expr));
  result->markType();
  auto t = expr->expr->getType();
  if (expr->getType() && expr->getType()->unify(t) < 0)
    ERROR(expr, "cannot unify {} and {}", *expr->getType(), *t);
  result->setType(t);
}

// TODO
void TransformExprVisitor::visit(const PtrExpr *expr) {
  ERROR(expr, "Todo ptr");
  result = N<PtrExpr>(transform(expr->expr));
  // result->setType(ctx.findInternal("ptr")->setGenerics(result->expr));
}

// TODO
void TransformExprVisitor::visit(const LambdaExpr *expr) {
  ERROR(expr, "to be done later");
}

// TODO
void TransformExprVisitor::visit(const YieldExpr *expr) {
  ERROR(expr, "to be done later");
  RETURN(YieldExpr, );
}

#undef RETURN
#define RETURN(T, ...)                                                         \
  (this->result =                                                              \
       fwdSrcInfo(make_unique<T>(__VA_ARGS__), stmt->getSrcInfo()));           \
  return

void TransformStmtVisitor::prepend(StmtPtr s) {
  prependStmts.push_back(transform(s));
}

StmtPtr TransformStmtVisitor::transform(const Stmt *stmt) {
  // if (stmt->getSrcInfo().file.find("scratch.seq") != string::npos)
  // fmt::print("<T> {}\n", *stmt);
  if (!stmt)
    return nullptr;
  TransformStmtVisitor v(ctx);
  v.setSrcInfo(stmt->getSrcInfo());

  DBG("{{ {} ## {}", *stmt, stmt->getSrcInfo().line);
  __level__++;
  stmt->accept(v);
  __level__--;
  DBG("  {} }}", *v.result);

  if (v.prependStmts.size()) {
    v.prependStmts.push_back(move(v.result));
    return N<SuiteStmt>(move(v.prependStmts));
  } else {
    return move(v.result);
  }
}

ExprPtr TransformStmtVisitor::transform(const Expr *expr) {
  if (!expr)
    return nullptr;
  vector<StmtPtr> old = move(prependStmts);
  prependStmts.clear();
  TransformExprVisitor v(ctx, *this);
  v.setSrcInfo(expr->getSrcInfo());
  DBG("[ {} :- {} # {}", *expr, expr->getType() ? expr->getType()->str() : "-",
      expr->getSrcInfo().line);
  __level__++;
  expr->accept(v);
  __level__--;
  DBG("  {} :- {} ]", *v.result,
      v.result->getType() ? v.result->getType()->str() : "-");

  for (auto &s : prependStmts)
    old.push_back(move(s));
  prependStmts = move(old);
  return move(v.result);
}

PatternPtr TransformStmtVisitor::transform(const Pattern *pat) {
  if (!pat)
    return nullptr;
  TransformPatternVisitor v(*this);
  pat->accept(v);
  return move(v.result);
}

TypePtr TransformStmtVisitor::realize(shared_ptr<FuncType> t) {
  auto it = ctx.funcRealizations.find(t->getCanonicalName());
  if (it != ctx.funcRealizations.end()) {
    auto it2 = it->second.find(t->str(true));
    if (it2 != it->second.end())
      return it2->second.first; // already realized
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

  assert(ctx.funcASTs.find(t->getCanonicalName()) != ctx.funcASTs.end());
  auto &ast = ctx.funcASTs[t->getCanonicalName()];
  // There is no AST linked to internal functions, so just ignore them
  bool isInternal =
      std::find(ast.second->attributes.begin(), ast.second->attributes.end(),
                "internal") != ast.second->attributes.end();

  DBG("======== BEGIN {} :- {} ========", t->getCanonicalName(), *t);
  auto realized = isInternal ? nullptr : realizeBlock(ast.second->suite.get());
  if (realized && !ctx.hasSetReturnType && t->ret) {
    auto u = ctx.returnType->unify(ctx.findInternal("void"));
    // TODO fix this
    assert(u >= 0);
  }
  DBG("======== END {} :- {} ========", t->getCanonicalName(), *t);

  vector<Param> args;
  assert(ast.second->args.size() == t->args.size());
  for (int i = 0; i < ast.second->args.size(); i++) {
    args.push_back({ast.second->args[i].name, nullptr, nullptr});
  }
  ctx.funcRealizations[t->getCanonicalName()][t->str(true)] =
      make_pair(t, Nx<FunctionStmt>(ast.second.get(), ast.second->name, nullptr,
                                    vector<string>(), move(args),
                                    move(realized), ast.second->attributes));
  ctx.returnType = old;
  ctx.hasSetReturnType = oldSeen;
  ctx.decreaseLevel();
  ctx.popBlock();
  return t;
}

StmtPtr TransformStmtVisitor::realizeBlock(const Stmt *stmt) {
  if (!stmt) {
    return nullptr;
  }
  StmtPtr result = nullptr;

  FILE *fo = fopen("out.htm", "w");
  fmt::print(fo, "<html><head><link rel=stylesheet href=code.css "
                 "/></head>\n<body>\n");
  // We keep running typecheck transformations until there are no more unbound
  // types. It is assumed that the unbound count will decrease in each
  // iteration--- if not, the program cannot be type-checked.
  // TODO: this can be probably optimized one day...
  int reachSize = ctx.activeUnbounds.size();
  for (int iter = 0, prevSize = INT_MAX; prevSize > reachSize; iter++) {
    DBG("----------------------------------------");
    TransformStmtVisitor v(ctx);
    if (result)
      result->accept(v);
    else
      stmt->accept(v);
    result = move(v.result);

    for (auto it = ctx.activeUnbounds.begin();
         it != ctx.activeUnbounds.end();) {
      if (!(*it)->isUnbound())
        it = ctx.activeUnbounds.erase(it);
      else
        ++it;
    }

    if (ctx.activeUnbounds.size() >= prevSize) {
      DBG("cannot resolve unbound variables");
      // ERROR(stmt, "cannot resolve unbound variables");
      break;
    }
    prevSize = ctx.activeUnbounds.size();
  }
  auto s = ast::FormatStmtVisitor(ctx).transform(result.get());
  fmt::print(fo, "<div class=code>\n{}\n</div>\n-------\n", s);

  fmt::print(fo, "</body></html>\n");
  fclose(fo);
  return result;
}

void TransformStmtVisitor::visit(const SuiteStmt *stmt) {
  result = N<SuiteStmt>(transform(stmt->stmts));
}

void TransformStmtVisitor::visit(const PassStmt *stmt) {
  result = N<PassStmt>();
}

void TransformStmtVisitor::visit(const BreakStmt *stmt) {
  result = N<BreakStmt>();
}

void TransformStmtVisitor::visit(const ContinueStmt *stmt) {
  result = N<ContinueStmt>();
}

void TransformStmtVisitor::visit(const ExprStmt *stmt) {
  result = N<ExprStmt>(transform(stmt->expr));
}

StmtPtr TransformStmtVisitor::addAssignment(const Expr *lhs, const Expr *rhs,
                                            const Expr *type, bool force) {
  // DBG("    --> {} := {}", *lhs, *rhs);
  // TODO
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
    // TODO type
    if (type)
      ERROR(type, "to do type");
    auto s = Nx<AssignStmt>(lhs, Nx<IdExpr>(l, l->value), transform(rhs),
                            transform(type), false, force);
    if (auto t = ctx.find(l->value)) {
      if (t->unify(s->rhs->getType()) < 0) {
        ERROR(rhs, "type {} is not compatible with {}", *t, *s->rhs->getType());
      }
      s->lhs->setType(t);
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

void TransformStmtVisitor::processAssignment(const Expr *lhs, const Expr *rhs,
                                             vector<StmtPtr> &stmts,
                                             bool force) {
  vector<Expr *> lefts;
  if (auto l = dynamic_cast<const TupleExpr *>(lhs)) {
    for (auto &i : l->items) {
      lefts.push_back(i.get());
    }
  } else if (auto l = dynamic_cast<const ListExpr *>(lhs)) {
    for (auto &i : l->items) {
      lefts.push_back(i.get());
    }
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
      if (dynamic_cast<UnpackExpr *>(lefts[st])) {
        error(lefts[st]->getSrcInfo(), "two unpack expressions in assignment");
      }
      processAssignment(
          lefts[st],
          Nx<IndexExpr>(rhs, rhs->clone(), Nx<IntExpr>(rhs, -lefts.size() + st))
              .release(),
          stmts, force);
    }
  }
}

void TransformStmtVisitor::visit(const AssignStmt *stmt) {
  // DBG("--> {}", *stmt);
  vector<StmtPtr> stmts;
  if (stmt->type) {
    if (auto i = CAST(stmt->lhs, IdExpr)) {
      stmts.push_back(
          addAssignment(stmt->lhs.get(), stmt->rhs.get(), stmt->type.get()));
    } else {
      ERROR(stmt, "only a single target can be annotated");
    }
  } else {
    processAssignment(stmt->lhs.get(), stmt->rhs.get(), stmts);
  }
  if (stmts.size() == 1) {
    this->result = move(stmts[0]);
  } else {
    result = N<SuiteStmt>(move(stmts));
  }
}

void TransformStmtVisitor::visit(const DelStmt *stmt) {
  if (auto expr = CAST(stmt->expr, IndexExpr)) {
    result = N<ExprStmt>(transform(N<CallExpr>(
        N<DotExpr>(expr->expr->clone(), "__delitem__"), expr->index->clone())));
  } else if (auto expr = CAST(stmt->expr, IdExpr)) {
    ctx.remove(expr->value);
    result = N<DelStmt>(transform(expr));
  } else {
    ERROR(stmt, "this expression cannot be deleted");
  }
}

// TODO
void TransformStmtVisitor::visit(const PrintStmt *stmt) {
  ERROR(stmt, "todo");
  RETURN(PrintStmt, transform(stmt->expr));
}

void TransformStmtVisitor::visit(const ReturnStmt *stmt) {
  ctx.hasSetReturnType = true;
  if (stmt->expr) {
    auto e = transform(stmt->expr);
    if (ctx.returnType->unify(e->getType()) < 0)
      ERROR(stmt, "incompatible return types: {} and {}", *e->getType(),
            *ctx.returnType);
    result = N<ReturnStmt>(move(e));
  } else {
    if (ctx.returnType->unify(ctx.findInternal("void")) < 0)
      ERROR(stmt, "incompatible return types: void and {}", *ctx.returnType);
    result = N<ReturnStmt>(nullptr);
  }
}

// TODO
void TransformStmtVisitor::visit(const YieldStmt *stmt) {
  ERROR(stmt, "todo");
  RETURN(YieldStmt, transform(stmt->expr));
}

void TransformStmtVisitor::visit(const AssertStmt *stmt) {
  result = N<AssertStmt>(transform(stmt->expr));
}

void TransformStmtVisitor::visit(const WhileStmt *stmt) {
  result = N<WhileStmt>(transform(TO_BOOL(stmt->cond)), transform(stmt->suite));
}

// TODO
void TransformStmtVisitor::visit(const ForStmt *stmt) {
  ERROR(stmt, "todo for");
  auto iter = transform(stmt->iter);
  StmtPtr suite;
  ExprPtr var;
  if (CAST(stmt->var, IdExpr)) {
    var = transform(stmt->var);
    suite = transform(stmt->suite);
  } else {
    string varName = getTemporaryVar("for");
    vector<StmtPtr> stmts;
    var = N<IdExpr>(varName);
    processAssignment(stmt->var.get(), var.get(), stmts, true);
    stmts.push_back(transform(stmt->suite));
    suite = N<SuiteStmt>(move(stmts));
  }
  RETURN(ForStmt, move(var), move(iter), move(suite));
}

void TransformStmtVisitor::visit(const IfStmt *stmt) {
  vector<IfStmt::If> ifs;
  for (auto &ifc : stmt->ifs)
    ifs.push_back({transform(TO_BOOL(ifc.cond)), transform(ifc.suite)});
  result = N<IfStmt>(move(ifs));
}

// TODO
void TransformStmtVisitor::visit(const MatchStmt *stmt) {
  ERROR(stmt, "todo match");
  RETURN(MatchStmt, transform(stmt->what), transform(stmt->patterns),
         transform(stmt->cases));
}

// TODO
void TransformStmtVisitor::visit(const ExtendStmt *stmt) {
  ERROR(stmt, "todo extend");
  auto suite = make_unique<SuiteStmt>(stmt->getSrcInfo());
  for (auto s : stmt->suite->getStatements()) {
    if (dynamic_cast<FunctionStmt *>(s)) {
      suite->stmts.push_back(transform(s));
    } else {
      error(s->getSrcInfo(), "types can be extended with functions only");
    }
  }
  RETURN(ExtendStmt, transform(stmt->what), move(suite));
}

// TODO
void TransformStmtVisitor::visit(const ImportStmt *stmt) {
  ERROR(stmt, "todo import");
  RETURN(ImportStmt, stmt->from, stmt->what);
}

// TODO
void TransformStmtVisitor::visit(const ExternImportStmt *stmt) {
  ERROR(stmt, "extern import");
  if (stmt->lang == "c" && stmt->from) {
    vector<StmtPtr> stmts;
    // ptr = _dlsym(FROM, WHAT)
    vector<ExprPtr> args;
    args.push_back(transform(stmt->from));
    args.push_back(N<StringExpr>(stmt->name.first));
    stmts.push_back(N<AssignStmt>(
        N<IdExpr>("ptr"), N<CallExpr>(N<IdExpr>("_dlsym"), move(args))));
    // f = function[ARGS](ptr)
    args.clear();
    args.push_back(stmt->ret ? transform(stmt->ret) : N<IdExpr>("void"));
    for (auto &a : stmt->args) {
      args.push_back(transform(a.type));
    }
    stmts.push_back(N<AssignStmt>(
        N<IdExpr>("f"), N<CallExpr>(N<IndexExpr>(N<IdExpr>("function"),
                                                 N<TupleExpr>(move(args))),
                                    N<IdExpr>("ptr"))));
    bool isVoid = true;
    if (stmt->ret) {
      if (auto f = CAST(stmt->ret, IdExpr)) {
        isVoid = f->value == "void";
      } else {
        isVoid = false;
      }
    }
    args.clear();
    int ia = 0;
    for (auto &a : stmt->args) {
      args.push_back(N<IdExpr>(a.name != "" ? a.name : format("$a{}", ia++)));
    }
    // return f(args)
    auto call = N<CallExpr>(N<IdExpr>("f"), move(args));
    if (!isVoid) {
      stmts.push_back(N<ReturnStmt>(move(call)));
    } else {
      stmts.push_back(N<ExprStmt>(move(call)));
    }
    // def WHAT(args):
    vector<Param> params;
    ia = 0;
    for (auto &a : stmt->args) {
      params.push_back(
          {a.name != "" ? a.name : format("$a{}", ia++), transform(a.type)});
    }
    RETURN(FunctionStmt,
           stmt->name.second != "" ? stmt->name.second : stmt->name.first,
           transform(stmt->ret), vector<string>(), move(params),
           N<SuiteStmt>(move(stmts)), vector<string>());
  } else if (stmt->lang == "c") {
    vector<Param> args;
    for (auto &a : stmt->args) {
      args.push_back({a.name, transform(a.type), transform(a.deflt)});
    }
    RETURN(ExternImportStmt, stmt->name, transform(stmt->from),
           transform(stmt->ret), move(args), stmt->lang);
  } else if (stmt->lang == "py") {
    vector<StmtPtr> stmts;
    string from = "";
    if (auto i = CAST(stmt->from, IdExpr)) {
      from = i->value;
    } else {
      ERROR(stmt, "invalid pyimport query");
    }
    auto call = N<CallExpr>( // _py_import(LIB)[WHAT].call ( ...
        N<DotExpr>(N<IndexExpr>(N<CallExpr>(N<IdExpr>("_py_import"),
                                            N<StringExpr>(from)),
                                N<StringExpr>(stmt->name.first)),
                   "call"),
        N<CallExpr>( // ... x.__to_py__() )
            N<DotExpr>(N<IdExpr>("x"), "__to_py__")));
    bool isVoid = true;
    if (stmt->ret) {
      if (auto f = CAST(stmt->ret, IdExpr)) {
        isVoid = f->value == "void";
      } else {
        isVoid = false;
      }
    }
    if (!isVoid) {
      // return TYP.__from_py__(call)
      stmts.push_back(N<ReturnStmt>(N<CallExpr>(
          N<DotExpr>(transform(stmt->ret), "__from_py__"), move(call))));
    } else {
      stmts.push_back(N<ExprStmt>(move(call)));
    }
    vector<Param> params;
    params.push_back({"x", nullptr, nullptr});
    RETURN(FunctionStmt,
           stmt->name.second != "" ? stmt->name.second : stmt->name.first,
           transform(stmt->ret), vector<string>(), move(params),
           N<SuiteStmt>(move(stmts)), vector<string>{"pyhandle"});
  } else {
    ERROR(stmt, "language {} not yet supported", stmt->lang);
  }
}

// TODO
void TransformStmtVisitor::visit(const TryStmt *stmt) {
  ERROR(stmt, "todo try");
  vector<TryStmt::Catch> catches;
  for (auto &c : stmt->catches) {
    catches.push_back({c.var, transform(c.exc), transform(c.suite)});
  }
  RETURN(TryStmt, transform(stmt->suite), move(catches),
         transform(stmt->finally));
}

// TODO
void TransformStmtVisitor::visit(const GlobalStmt *stmt) {
  ERROR(stmt, "todo global");
  RETURN(GlobalStmt, stmt->var);
}

void TransformStmtVisitor::visit(const ThrowStmt *stmt) {
  result = N<ThrowStmt>(transform(stmt->expr));
}

void TransformStmtVisitor::visit(const FunctionStmt *stmt) {
  auto canonicalName =
      ctx.getCanonicalName(ctx.prefix + stmt->name, stmt->getSrcInfo());
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
      auto t = transform(a.type);
      argTypes.push_back(make_pair(
          a.name, a.type ? t->getType() : ctx.addUnbound(getSrcInfo(), false)));
      args.push_back({a.name, move(t), transform(a.deflt)});
    }
    auto ret = stmt->ret ? transform(stmt->ret)->getType()
                         : ctx.addUnbound(getSrcInfo(), false);
    ctx.decreaseLevel();
    ctx.popBlock();

    auto type = make_shared<FuncType>(stmt->name, canonicalName, genericTypes,
                                      argTypes, ret);
    auto t = type->generalize(ctx.level);
    DBG("* [function] {} :- {}", ctx.prefix + stmt->name, *t);
    ctx.add(ctx.prefix + stmt->name, t);

    auto fp = N<FunctionStmt>(stmt->name, nullptr, stmt->generics, move(args),
                              stmt->suite, stmt->attributes);
    ctx.funcASTs[canonicalName] = make_pair(t, move(fp));
  }
  result = N<FunctionStmt>(stmt->name, nullptr, vector<string>(),
                           vector<Param>(), nullptr, stmt->attributes);
}

void TransformStmtVisitor::visit(const ClassStmt *stmt) {
  auto canonicalName = ctx.getCanonicalName(stmt->name, stmt->getSrcInfo());
  vector<StmtPtr> suite;
  if (ctx.classASTs.find(canonicalName) == ctx.classASTs.end()) {
    // Classes are handled differently as they can contain recursive references
    if (!stmt->isRecord) {
      vector<pair<int, TypePtr>> genericTypes;
      vector<string> generics;
      for (auto &g : stmt->generics) {
        auto t = make_shared<LinkType>(LinkType::Generic, ctx.unboundCount);
        genericTypes.push_back(make_pair(ctx.unboundCount, t));

        auto tp = make_shared<LinkType>(LinkType::Unbound, ctx.unboundCount,
                                        ctx.level);
        ctx.add(g, tp, true);
        ctx.unboundCount++;
      }
      auto ct = make_shared<ClassType>(stmt->name, canonicalName, genericTypes);
      ctx.add(ctx.prefix + stmt->name, ct, true);
      ctx.classASTs[canonicalName] =
          make_pair(ct, nullptr); // TODO: fix nullptr
      DBG("* [class] {} :- {}", ctx.prefix + stmt->name, *ct);

      ctx.increaseLevel();
      for (auto &a : stmt->args) {
        assert(a.type);
        ctx.classMembers[canonicalName][a.name] =
            transform(a.type)->getType()->generalize(ctx.level);
        DBG("* [class] [member.{}] :- {}", a.name,
            *ctx.classMembers[canonicalName][a.name]);
      }
      auto oldPrefix = ctx.prefix;
      ctx.prefix += stmt->name + ".";
      // Generate __new__
      auto codeType =
          format("{}{}", stmt->name,
                 stmt->generics.size()
                     ? format("[{}]", fmt::join(stmt->generics, ", "))
                     : "");
      // DBG("{}", codeNew);
      auto addMethod = [&](auto s) {
        if (auto f = dynamic_cast<FunctionStmt *>(s)) {
          suite.push_back(transform(s));
          auto t =
              dynamic_pointer_cast<FuncType>(ctx.find(ctx.prefix + f->name));
          assert(t);
          t->setImplicits(genericTypes);
          ctx.classMethods[canonicalName][f->name] = t;
        } else {
          ERROR(s, "types can only contain functions");
        };
      };
      auto codeNew = format("@internal\ndef __new__() -> {}: pass", codeType);
      auto methodNew = parse_code(ctx.filename, codeNew);
      for (auto s : methodNew->getStatements())
        addMethod(s);
      for (auto s : stmt->suite->getStatements())
        addMethod(s);
      ctx.decreaseLevel();
      for (auto &g : stmt->generics) {
        // Generalize in place
        auto t = dynamic_pointer_cast<LinkType>(ctx.find(g));
        assert(t && t->isUnbound());
        t->kind = LinkType::Generic;
        ctx.remove(g);
      }
      ctx.prefix = oldPrefix;
    } else {
      vector<pair<string, TypePtr>> argTypes;
      for (auto &a : stmt->args) {
        assert(a.type);
        auto t = transform(a.type)->getType();
        argTypes.push_back({a.name, t});
        ctx.classMembers[canonicalName][a.name] = t;
      }
      auto ct = make_shared<RecordType>(stmt->name, canonicalName, argTypes);
      ctx.add(stmt->name, ct, true);
      ctx.classASTs[canonicalName] =
          make_pair(ct, nullptr);    // TODO: fix nullptr
      ctx.prefix = stmt->name + "."; // TODO: supports only one nesting level
      // Add other statements
      for (auto s : stmt->suite->getStatements()) {
        if (auto f = dynamic_cast<FunctionStmt *>(s)) {
          suite.push_back(transform(s));
          auto t =
              dynamic_pointer_cast<FuncType>(ctx.find(ctx.prefix + f->name));
          assert(t);
          ctx.classMethods[canonicalName][f->name] = t;
        } else {
          ERROR(s, "types can only contain functions");
        }
      }
      ctx.prefix = "";
    }
  }
  result = N<ClassStmt>(stmt->isRecord, stmt->name, vector<string>(),
                        vector<Param>(), N<SuiteStmt>(move(suite)));
}

// TODO
void TransformStmtVisitor::visit(const DeclareStmt *stmt) {
  ERROR(stmt, "todo declare");
  RETURN(DeclareStmt, Param{stmt->param.name, transform(stmt->param.type),
                            transform(stmt->param.deflt)});
}

// TODO
void TransformStmtVisitor::visit(const AssignEqStmt *stmt) {
  ERROR(stmt, "todo assigneq");
  RETURN(
      AssignStmt, transform(stmt->lhs),
      N<BinaryExpr>(transform(stmt->lhs), stmt->op, transform(stmt->rhs), true),
      nullptr, true);
}

// TODO
void TransformStmtVisitor::visit(const YieldFromStmt *stmt) {
  auto var = getTemporaryVar("yield");
  vector<StmtPtr> stmts;
  stmts.push_back(N<YieldStmt>(N<IdExpr>(var)));
  RETURN(ForStmt, N<IdExpr>(var), transform(stmt->expr),
         N<SuiteStmt>(move(stmts)));
}

// TODO
void TransformStmtVisitor::visit(const WithStmt *stmt) {
  ERROR(stmt, "todo with");
  if (!stmt->items.size()) {
    ERROR(stmt, "malformed with statement");
  }
  vector<StmtPtr> content;
  for (int i = stmt->items.size() - 1; i >= 0; i--) {
    vector<StmtPtr> internals;
    string var = stmt->vars[i] == "" ? getTemporaryVar("with") : stmt->vars[i];
    internals.push_back(
        N<AssignStmt>(N<IdExpr>(var), transform(stmt->items[i])));
    internals.push_back(
        N<ExprStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "__enter__"))));
    internals.push_back(
        N<TryStmt>(content.size() ? transform(N<SuiteStmt>(move(content)))
                                  : transform(stmt->suite),
                   vector<TryStmt::Catch>(),
                   transform(N<ExprStmt>(
                       N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "__exit__"))))));
    content = move(internals);
  }
  vector<IfStmt::If> ifs;
  ifs.push_back({N<BoolExpr>(true), N<SuiteStmt>(move(content))});
  RETURN(IfStmt, move(ifs));
}

// TODO
void TransformStmtVisitor::visit(const PyDefStmt *stmt) {
  ERROR(stmt, "todo pydef");
  // _py_exec(""" str """)
  vector<string> args;
  for (auto &a : stmt->args) {
    args.push_back(a.name);
  }
  string code = format("def {}({}):\n{}\n", stmt->name, fmt::join(args, ", "),
                       stmt->code);
  // DBG("py code:\n{}", code);
  vector<StmtPtr> stmts;
  stmts.push_back(
      N<ExprStmt>(N<CallExpr>(N<IdExpr>("_py_exec"), N<StringExpr>(code))));
  // from __main__ pyimport foo () -> ret
  stmts.push_back(transform(
      N<ExternImportStmt>(make_pair(stmt->name, ""), N<IdExpr>("__main__"),
                          transform(stmt->ret), vector<Param>(), "py")));
  RETURN(SuiteStmt, move(stmts));
}

#undef RETURN
#define RETURN(T, ...)                                                         \
  (this->result = fwdSrcInfo(make_unique<T>(__VA_ARGS__), pat->getSrcInfo())); \
  return

TransformPatternVisitor::TransformPatternVisitor(
    TransformStmtVisitor &stmtVisitor)
    : stmtVisitor(stmtVisitor) {}

PatternPtr TransformPatternVisitor::transform(const Pattern *ptr) {
  TransformPatternVisitor v(stmtVisitor);
  ptr->accept(v);
  return move(v.result);
}

vector<PatternPtr>
TransformPatternVisitor::transform(const vector<PatternPtr> &pats) {
  vector<PatternPtr> r;
  for (auto &e : pats) {
    r.push_back(transform(e));
  }
  return r;
}

void TransformPatternVisitor::visit(const StarPattern *pat) {
  RETURN(StarPattern, );
}

void TransformPatternVisitor::visit(const IntPattern *pat) {
  RETURN(IntPattern, pat->value);
}

void TransformPatternVisitor::visit(const BoolPattern *pat) {
  RETURN(BoolPattern, pat->value);
}

void TransformPatternVisitor::visit(const StrPattern *pat) {
  RETURN(StrPattern, pat->value);
}

void TransformPatternVisitor::visit(const SeqPattern *pat) {
  RETURN(SeqPattern, pat->value);
}

void TransformPatternVisitor::visit(const RangePattern *pat) {
  RETURN(RangePattern, pat->start, pat->end);
}

void TransformPatternVisitor::visit(const TuplePattern *pat) {
  RETURN(TuplePattern, transform(pat->patterns));
}

void TransformPatternVisitor::visit(const ListPattern *pat) {
  RETURN(ListPattern, transform(pat->patterns));
}

void TransformPatternVisitor::visit(const OrPattern *pat) {
  RETURN(OrPattern, transform(pat->patterns));
}

void TransformPatternVisitor::visit(const WildcardPattern *pat) {
  RETURN(WildcardPattern, pat->var);
}

void TransformPatternVisitor::visit(const GuardedPattern *pat) {
  RETURN(GuardedPattern, transform(pat->pattern),
         stmtVisitor.transform(pat->cond));
}

void TransformPatternVisitor::visit(const BoundPattern *pat) {
  RETURN(BoundPattern, pat->var, transform(pat->pattern));
}

} // namespace ast
} // namespace seq
