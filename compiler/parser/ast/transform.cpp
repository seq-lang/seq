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

// TODO: get rid of this macro
#define RETURN(T, ...)                                                         \
  (this->result =                                                              \
       fwdSrcInfo(make_unique<T>(__VA_ARGS__), expr->getSrcInfo()));           \
  return
// TODO: unify these into some sort of template monstrosity
#define E(T, ...) make_unique<T>(__VA_ARGS__)
#define EP(T, ...) fwdSrcInfo(make_unique<T>(__VA_ARGS__), expr->getSrcInfo())
#define EPX(e, T, ...) fwdSrcInfo(make_unique<T>(__VA_ARGS__), e->getSrcInfo())
#define S(T, ...) make_unique<T>(__VA_ARGS__)
#define SP(T, ...) fwdSrcInfo(make_unique<T>(__VA_ARGS__), stmt->getSrcInfo())
#define SPX(s, T, ...) fwdSrcInfo(make_unique<T>(__VA_ARGS__), s->getSrcInfo())
// TODO: use hack from common.h to glob the comma from __VA_ARGS__
#define ERROR(s, ...) error(s->getSrcInfo(), __VA_ARGS__)
#define CAST(s, T) dynamic_cast<T *>(s.get())

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

shared_ptr<LinkType> TypeContext::addUnbound(bool setActive) {
  auto t = make_shared<LinkType>(LinkType::Unbound, unboundCount, level);
  if (setActive)
    activeUnbounds.insert(t);
  unboundCount++;
  return t;
}

TypePtr TypeContext::instantiate(TypePtr type) {
  return instantiate(type, vector<pair<int, TypePtr>>());
}

TypePtr TypeContext::instantiate(TypePtr type,
                                 const vector<pair<int, TypePtr>> &generics) {
  std::unordered_map<int, TypePtr> cache;
  for (auto &g : generics) {
    cache[g.first] = g.second;
  }
  auto t = type->instantiate(level, unboundCount, cache);
  for (auto &i : cache) {
    if (i.second->isUnbound()) {
      activeUnbounds.insert(i.second);
    }
  }
  return t;
}

shared_ptr<FuncType> TypeContext::findMethod(const ClassType *type,
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

TypePtr TypeContext::findMember(const ClassType *type, const string &member) {
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
      result.push_back({i.first, i.second.get()});
    }
  }
  return result;
}

TransformExprVisitor::TransformExprVisitor(TypeContext &ctx,
                                           TransformStmtVisitor &sv)
    : ctx(ctx), stmtVisitor(sv) {}

ExprPtr TransformExprVisitor::transform(const Expr *expr) {
  TransformExprVisitor v(ctx, stmtVisitor);
  expr->accept(v);
  return move(v.result);
}

vector<ExprPtr> TransformExprVisitor::transform(const vector<ExprPtr> &exprs) {
  vector<ExprPtr> r;
  for (auto &e : exprs) {
    r.push_back(transform(e));
  }
  return r;
}

void TransformExprVisitor::visit(const NoneExpr *expr) {
  result = EP(NoneExpr, );
  result->setType(expr->getType() ? expr->getType() : ctx.addUnbound());
}

void TransformExprVisitor::visit(const BoolExpr *expr) {
  result = EP(BoolExpr, expr->value);
  result->setType(make_shared<LinkType>(ctx.findInternal("bool")));
}

void TransformExprVisitor::visit(const IntExpr *expr) {
  result = EP(IntExpr, expr->value);
  result->setType(make_shared<LinkType>(ctx.findInternal("int")));
}

void TransformExprVisitor::visit(const FloatExpr *expr) {
  result = EP(FloatExpr, expr->value);
  result->setType(make_shared<LinkType>(ctx.findInternal("float")));
}

void TransformExprVisitor::visit(const StringExpr *expr) {
  result = EP(StringExpr, expr->value);
  result->setType(make_shared<LinkType>(ctx.findInternal("str")));
}

void TransformExprVisitor::visit(const FStringExpr *expr) {
  int braceCount = 0, braceStart = 0;
  vector<ExprPtr> items;
  for (int i = 0; i < expr->value.size(); i++) {
    if (expr->value[i] == '{') {
      if (braceStart < i) {
        items.push_back(
            EP(StringExpr, expr->value.substr(braceStart, i - braceStart)));
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
          items.push_back(EP(StringExpr, format("{}=", code)));
        }
        items.push_back(
            EP(CallExpr, EP(IdExpr, "str"), parse_expr(code, offset)));
      }
      braceStart = i + 1;
    }
  }
  if (braceCount) {
    ERROR(expr, "f-string braces not balanced");
  }
  if (braceStart != expr->value.size()) {
    items.push_back(
        EP(StringExpr,
           expr->value.substr(braceStart, expr->value.size() - braceStart)));
  }
  result = transform(EP(CallExpr, EP(DotExpr, EP(IdExpr, "str"), "cat"),
                        EP(ListExpr, move(items))));
}

void TransformExprVisitor::visit(const KmerExpr *expr) {
  result = transform(
      EP(CallExpr,
         EP(IndexExpr, EP(IdExpr, "Kmer"), EP(IntExpr, expr->value.size())),
         EP(SeqExpr, expr->value)));
}

void TransformExprVisitor::visit(const SeqExpr *expr) {
  if (expr->prefix == "p") {
    result = transform(
        EP(CallExpr, EP(IdExpr, "pseq"), EP(StringExpr, expr->value)));
  } else if (expr->prefix == "s") {
    result = EP(SeqExpr, expr->value, expr->prefix);
    result->setType(make_shared<LinkType>(ctx.findInternal("seq")));
  } else {
    ERROR(expr, "invalid seq prefix '{}'", expr->prefix);
  }
}

void TransformExprVisitor::visit(const IdExpr *expr) {
  bool isType = false;
  auto type = ctx.find(expr->value, &isType);
  if (!type) {
    ERROR(expr, "identifier '{}' not found", expr->value);
  }
  result = EP(IdExpr, expr->value);
  if (isType)
    result->markType();
  result->setType(expr->getType() ? expr->getType() : ctx.instantiate(type));
}

void TransformExprVisitor::visit(const UnpackExpr *expr) {
  result = transform(EP(CallExpr, EP(IdExpr, "list"), transform(expr->what)));
}

void TransformExprVisitor::visit(const TupleExpr *expr) {
  auto e = EP(TupleExpr, transform(expr->items));
  vector<pair<string, TypePtr>> types;
  for (auto &i : e->items)
    types.push_back({"", i->getType()});
  auto t = make_shared<LinkType>(make_shared<RecordType>("", "", types));
  result = move(e);
  result->setType(t);
}

void TransformExprVisitor::visit(const ListExpr *expr) {
  string listVar = getTemporaryVar("list");
  stmtVisitor.prepend(
      SPX(expr, AssignStmt, EP(IdExpr, listVar),
          EP(CallExpr, EP(IdExpr, "list"), EP(IntExpr, expr->items.size()))));
  for (int i = 0; i < expr->items.size(); i++) {
    stmtVisitor.prepend(
        SPX(expr, ExprStmt,
            EP(CallExpr, EP(DotExpr, EP(IdExpr, listVar), "append"),
               transform(expr->items[i]))));
  }
  result = transform(EP(IdExpr, listVar));
}

void TransformExprVisitor::visit(const SetExpr *expr) {
  string setVar = getTemporaryVar("set");
  stmtVisitor.prepend(SPX(expr, AssignStmt, EP(IdExpr, setVar),
                          EP(CallExpr, EP(IdExpr, "set"))));
  for (int i = 0; i < expr->items.size(); i++) {
    stmtVisitor.prepend(SPX(expr, ExprStmt,
                            EP(CallExpr, EP(DotExpr, EP(IdExpr, setVar), "add"),
                               transform(expr->items[i]))));
  }
  result = transform(EP(IdExpr, setVar));
}

void TransformExprVisitor::visit(const DictExpr *expr) {
  string dictVar = getTemporaryVar("dict");
  stmtVisitor.prepend(SPX(expr, AssignStmt, EP(IdExpr, dictVar),
                          EP(CallExpr, EP(IdExpr, "dict"))));
  for (int i = 0; i < expr->items.size(); i++) {
    vector<ExprPtr> args;
    args.push_back(transform(expr->items[i].key));
    args.push_back(transform(expr->items[i].value));
    stmtVisitor.prepend(
        SPX(expr, ExprStmt,
            EP(CallExpr, EP(DotExpr, EP(IdExpr, dictVar), "__setitem__"),
               move(args))));
  }
  result = transform(EP(IdExpr, dictVar));
}

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
  auto e = EP(IfExpr, transform(expr->cond), transform(expr->eif),
              transform(expr->eelse));
  // TODO: better handling of boolean conditions (use __bool__ if needed)
  if (e->cond->getType()->unify(ctx.findInternal("bool")) < 0) {
    ERROR(expr, "expected bool expression, got {}", *e->cond->getType());
  }
  if (e->eif->getType()->unify(e->eelse->getType()) < 0) {
    ERROR(expr, "type {} is not compatible with {}", e->eif->getType()->str(),
          e->eelse->getType()->str());
  }
  result = move(e);
  result->setType(expr->getType() ? expr->getType() : e->eif->getType());
}

void TransformExprVisitor::visit(const UnaryExpr *expr) {
  if (expr->op == "!") { // Special case
    auto e = EP(UnaryExpr, expr->op, transform(expr->expr));
    if (e->expr->getType()->unify(ctx.findInternal("bool")) < 0) {
      ERROR(expr, "expected bool expression, got {}", *e->expr->getType());
    }
    result = move(e);
    result->setType(expr->getType() ? expr->getType()
                                    : ctx.findInternal("bool"));
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
  result = transform(EP(
      CallExpr, EP(DotExpr, transform(expr->expr), format("__{}__", magic))));
}

void TransformExprVisitor::visit(const BinaryExpr *expr) {
  if (expr->op == "&&" || expr->op == "||") { // Special case
    auto e = EP(BinaryExpr, transform(expr->lexpr), expr->op,
                transform(expr->rexpr));
    if (e->lexpr->getType()->unify(ctx.findInternal("bool")) < 0) {
      ERROR(e->lexpr, "expected bool expression, got {}", *e->lexpr->getType());
    }
    if (e->rexpr->getType()->unify(ctx.findInternal("bool")) < 0) {
      ERROR(e->rexpr, "expected bool expression, got {}", *e->rexpr->getType());
    }
    result = move(e);
    result->setType(expr->getType() ? expr->getType()
                                    : ctx.findInternal("bool"));
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
  result = transform(
      EP(CallExpr, EP(DotExpr, transform(expr->lexpr), format("__{}__", magic)),
         transform(expr->rexpr)));
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
        if (it->isType()) {
          generics.push_back(it->getType());
        } else {
          ERROR(it, "{} is not a type", *it);
        }
      }
    } else {
      auto it = transform(expr->index);
      if (it->isType()) {
        generics.push_back(it->getType());
      } else {
        ERROR(it, "{} is not a type", *it);
      }
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
    result = EP(TypeOfExpr, move(e));
    result->markType();
    result->setType(t);
  }
  /* TODO: handle tuple access */
  else {
    result = transform(EP(CallExpr, EP(DotExpr, move(e), "__getitem__"),
                          transform(expr->index)));
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
          e = EP(IdExpr, m->getCanonicalName());
          e->setType(ctx.instantiate(m, c->generics));
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
  // Handle other arguments, if any
  for (auto &i : expr->args)
    args.push_back({i.name, transform(i.value)});

  // If constructor, replace with appropriate calls
  if (e->isType()) {
    if (!e->getType()->getClass()) {
      ERROR(e, "cannot call non-type");
    }
    // DBG("[call] transform {} to __new__", *e->getType());
    string name = e->getType()->getClass()->getCanonicalName();
    string var = getTemporaryVar("typ");
    stmtVisitor.prepend(
        SPX(expr, AssignStmt, EP(IdExpr, var),
            EP(CallExpr, EP(DotExpr, EP(IdExpr, name), "__new__"))));
    stmtVisitor.prepend(SPX(
        expr, ExprStmt,
        EP(CallExpr, EP(DotExpr, EP(IdExpr, var), "__init__"), move(args))));
    result = transform(EP(IdExpr, var));
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
    DBG("[call] {} ({} + {}) :- {}", t->getCanonicalName(), torig, params, *t);
    result = EP(CallExpr, move(e), move(args));
    result->setType(make_shared<LinkType>(t->ret));
  } else { // will be handled later on
    result = EP(CallExpr, move(e), move(args));
    result->setType(expr->getType() ? expr->getType() : ctx.addUnbound());
    // DBG("[call] [unbound] :- {}", *result->getType());
  }
}

void TransformExprVisitor::visit(const DotExpr *expr) {
  // TODO: handle imports

  auto lhs = transform(expr->expr);
  TypePtr typ = nullptr;
  if (lhs->getType()->isUnbound()) {
    typ = expr->getType() ? expr->getType() : ctx.addUnbound();
    DBG("[dot] {} . {} :- {}", *lhs->getType(), expr->member, *typ);
  } else if (auto c = lhs->getType()->getClass()) {
    // DBG("?? {} {} {} {}", *expr, *lhs, lhs->isType(), *lhs->getType());
    if (auto m = ctx.findMethod(c, expr->member)) {
      if (lhs->isType()) {
        typ = ctx.instantiate(m, c->generics);
        DBG("[dot] {} . {} :- {}  # t_raw: {}", *lhs->getType(), expr->member,
            *typ, *m);
      } else {
        // TODO: for now, this method cannot handle obj.method expression
        // (CallExpr does that for obj.method() expressions). Should be fixed
        // later...
        DBG("[dot] {} . {} :- NULL", *lhs->getType(), expr->member);
      }
    } else if (auto m = ctx.findMember(c, expr->member)) { // is member
      typ = ctx.instantiate(m, c->generics);
      DBG("[dot] {} . {} :- {}  # m_raw: {}", *lhs->getType(), expr->member,
          *typ, *m);
    } else {
      ERROR(expr, "cannot find '{}' in {}", expr->member, *lhs->getType());
    }
  } else {
    ERROR(expr, "cannot search for '{}' in {}", expr->member, *lhs->getType());
  }
  result = EP(DotExpr, move(lhs), expr->member);
  result->setType(typ);
}

void TransformExprVisitor::visit(const SliceExpr *expr) {
  string prefix;
  if (!expr->st && expr->ed) {
    prefix = "l";
  } else if (expr->st && !expr->ed) {
    prefix = "r";
  } else if (!expr->st && !expr->ed) {
    prefix = "e";
  }
  if (expr->step) {
    prefix += "s";
  }
  vector<ExprPtr> args;
  if (expr->st) {
    args.push_back(transform(expr->st));
  }
  if (expr->ed) {
    args.push_back(transform(expr->ed));
  }
  if (expr->step) {
    args.push_back(transform(expr->step));
  }
  if (!args.size()) {
    args.push_back(EP(IntExpr, 0));
  }
  result = transform(EP(CallExpr, EP(IdExpr, prefix + "slice"), move(args)));
}

void TransformExprVisitor::visit(const EllipsisExpr *expr) {
  result = EP(EllipsisExpr, );
}

void TransformExprVisitor::visit(const TypeOfExpr *expr) {
  // TODO: cast to IdExpr
  result = EP(TypeOfExpr, transform(expr->expr));
  result->markType();
  auto t = expr->expr->getType();
  result->setType(t);
}

// TODO
void TransformExprVisitor::visit(const PtrExpr *expr) {
  ERROR(expr, "Todo ptr");
  result = EP(PtrExpr, transform(expr->expr));
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
  if (!stmt) {
    return nullptr;
  }
  TransformStmtVisitor v(ctx);
  stmt->accept(v);
  if (v.prependStmts.size()) {
    v.prependStmts.push_back(move(v.result));
    return make_unique<SuiteStmt>(move(v.prependStmts));
  } else {
    return move(v.result);
  }
}

ExprPtr TransformStmtVisitor::transform(const Expr *expr) {
  if (!expr) {
    return nullptr;
  }
  vector<StmtPtr> old = move(prependStmts);
  prependStmts.clear();
  TransformExprVisitor v(ctx, *this);
  expr->accept(v);
  for (auto &s : prependStmts) {
    old.push_back(move(s));
  }
  prependStmts = move(old);
  return move(v.result);
}

PatternPtr TransformStmtVisitor::transform(const Pattern *pat) {
  if (!pat) {
    return nullptr;
  }
  TransformPatternVisitor v(*this);
  pat->accept(v);
  return move(v.result);
}

void TransformStmtVisitor::realize(FuncType *t) {
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

  DBG("======== BEGIN {} ========", t->getCanonicalName());
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
  ctx.funcRealizations[t->getCanonicalName()][t->str()] =
      SPX(ast.second, FunctionStmt, ast.second->name, nullptr, vector<string>(),
          move(args), move(realized), ast.second->attributes);
  ctx.returnType = old;
  ctx.hasSetReturnType = oldSeen;
  ctx.decreaseLevel();
  ctx.popBlock();
}

StmtPtr TransformStmtVisitor::realizeBlock(const Stmt *stmt) {
  if (!stmt) {
    return nullptr;
  }
  StmtPtr result;

  FILE *fo = fopen("out.htm", "w");
  fmt::print(fo, "<html><head><link rel=stylesheet href=code.css "
                 "/></head>\n<body>\n");
  // We keep running typecheck transformations until there are no more unbound
  // types. It is assumed that the unbound count will decrease in each
  // iteration--- if not, the program cannot be type-checked.
  // TODO: this can be probably optimized one day...
  int reachSize = ctx.activeUnbounds.size();
  for (int iter = 0, prevSize = INT_MAX; prevSize > reachSize; iter++) {
    DBG("--------");
    TransformStmtVisitor v(ctx);
    stmt->accept(v);
    result = move(v.result);

    for (auto it = ctx.activeUnbounds.begin();
         it != ctx.activeUnbounds.end();) {
      if (!(*it)->isUnbound())
        it = ctx.activeUnbounds.erase(it);
      else
        ++it;
    }

    auto s = ast::FormatStmtVisitor(ctx).transform(result.get());
    fmt::print(fo, "<div class=code>\n{}\n</div>\n-------\n", s);

    if (ctx.activeUnbounds.size() >= prevSize) {
      DBG("cannot resolve unbound variables");
      // ERROR(stmt, "cannot resolve unbound variables");
      break;
    }
    prevSize = ctx.activeUnbounds.size();
  }
  fmt::print(fo, "</body></html>\n");
  fclose(fo);
  return result;
}

void TransformStmtVisitor::visit(const SuiteStmt *stmt) {
  vector<StmtPtr> result;
  for (auto &i : stmt->stmts) {
    if (auto s = transform(i)) {
      result.push_back(move(s));
    }
  }
  RETURN(SuiteStmt, move(result));
}

void TransformStmtVisitor::visit(const PassStmt *stmt) {
  result = SP(PassStmt, );
}

void TransformStmtVisitor::visit(const BreakStmt *stmt) {
  result = SP(BreakStmt, );
}

void TransformStmtVisitor::visit(const ContinueStmt *stmt) {
  result = SP(ContinueStmt, );
}

void TransformStmtVisitor::visit(const ExprStmt *stmt) {
  result = SP(ExprStmt, transform(stmt->expr));
}

StmtPtr TransformStmtVisitor::addAssignment(const Expr *lhs, const Expr *rhs,
                                            const Expr *type, bool force) {
  // DBG("    --> {} := {}", *lhs, *rhs);
  // TODO
  if (auto l = dynamic_cast<const IndexExpr *>(lhs)) {
    vector<ExprPtr> args;
    args.push_back(transform(l->index));
    args.push_back(transform(rhs));
    return transform(SPX(
        lhs, ExprStmt,
        EPX(lhs, CallExpr, EPX(lhs, DotExpr, transform(l->expr), "__setitem__"),
            move(args))));
  } else if (auto l = dynamic_cast<const DotExpr *>(lhs)) {
    // TODO Member Expr
    return SPX(lhs, AssignStmt, transform(lhs), transform(rhs));
  } else if (auto l = dynamic_cast<const IdExpr *>(lhs)) {
    // TODO type
    if (type)
      ERROR(type, "to do type");
    auto s = SPX(lhs, AssignStmt, EPX(l, IdExpr, l->value), transform(rhs),
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
    auto newRhs = EPX(rhs, IdExpr, var).release();
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
    // TODO: RHS here (and below) will be transformed twice in order to avoid
    // messing up with unique_ptr. Better solution needed?
    processAssignment(
        lefts[st],
        EPX(rhs, IndexExpr, transform(rhs), EPX(rhs, IntExpr, st)).release(),
        stmts, force);
  }
  if (unpack) {
    processAssignment(unpack->what.get(),
                      EPX(rhs, IndexExpr, transform(rhs),
                          EPX(rhs, SliceExpr, EPX(rhs, IntExpr, st),
                              lefts.size() == st + 1
                                  ? nullptr
                                  : EPX(rhs, IntExpr, -lefts.size() + st + 1),
                              nullptr))
                          .release(),
                      stmts, force);
    st += 1;
    for (; st < lefts.size(); st++) {
      if (dynamic_cast<UnpackExpr *>(lefts[st])) {
        error(lefts[st]->getSrcInfo(), "two unpack expressions in assignment");
      }
      processAssignment(lefts[st],
                        EPX(rhs, IndexExpr, transform(rhs),
                            EPX(rhs, IntExpr, -lefts.size() + st))
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
    result = SP(SuiteStmt, move(stmts));
  }
}

void TransformStmtVisitor::visit(const DelStmt *stmt) {
  if (auto expr = CAST(stmt->expr, IndexExpr)) {
    result = SP(
        ExprStmt,
        transform(EPX(stmt, CallExpr,
                      EPX(stmt, DotExpr, transform(expr->expr), "__delitem__"),
                      transform(expr->index))));
  } else if (auto expr = CAST(stmt->expr, IdExpr)) {
    ctx.remove(expr->value);
    result = SP(DelStmt, transform(expr));
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
  if (stmt->expr) {
    auto e = transform(stmt->expr);
    ctx.hasSetReturnType = true;
    if (ctx.returnType->unify(e->getType()) < 0) {
      ERROR(stmt, "incompatible return types: {} and {}", *e->getType(),
            *ctx.returnType);
    }
    result = SP(ReturnStmt, move(e));
  } else {
    result = SP(ReturnStmt, nullptr);
  }
}

// TODO
void TransformStmtVisitor::visit(const YieldStmt *stmt) {
  ERROR(stmt, "todo");
  RETURN(YieldStmt, transform(stmt->expr));
}

void TransformStmtVisitor::visit(const AssertStmt *stmt) {
  result = SP(AssertStmt, transform(stmt->expr));
}

// TODO
// void TransformStmtVisitor::visit(const TypeAliasStmt *stmt) {
//   ERROR(stmt, "deprecated");
//   RETURN(TypeAliasStmt, stmt->name, transform(stmt->expr));
// }

void TransformStmtVisitor::visit(const WhileStmt *stmt) {
  auto cond = transform(stmt->cond);
  if (cond->getType()->unify(ctx.findInternal("bool")) < 0) {
    ERROR(cond, "expected bool expression, got {}", *cond->getType());
  }
  result = SP(WhileStmt, move(cond), transform(stmt->suite));
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
    var = EPX(stmt, IdExpr, varName);
    processAssignment(stmt->var.get(), var.get(), stmts, true);
    stmts.push_back(transform(stmt->suite));
    suite = SP(SuiteStmt, move(stmts));
  }
  RETURN(ForStmt, move(var), move(iter), move(suite));
}

void TransformStmtVisitor::visit(const IfStmt *stmt) {
  vector<IfStmt::If> ifs;
  for (auto &ifc : stmt->ifs) {
    auto cond = transform(ifc.cond);
    if (cond->getType()->unify(ctx.findInternal("bool")) < 0) {
      ERROR(cond, "expected bool expression, got {}", *cond->getType());
    }
    ifs.push_back({move(cond), transform(ifc.suite)});
  }
  result = SP(IfStmt, move(ifs));
}

// TODO
void TransformStmtVisitor::visit(const MatchStmt *stmt) {
  ERROR(stmt, "todo match");
  vector<pair<PatternPtr, StmtPtr>> cases;
  for (auto &c : stmt->cases) {
    cases.push_back({transform(c.first), transform(c.second)});
  }
  RETURN(MatchStmt, transform(stmt->what), move(cases));
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
    args.push_back(EPX(stmt, StringExpr, stmt->name.first));
    stmts.push_back(
        SP(AssignStmt, EPX(stmt, IdExpr, "ptr"),
           EPX(stmt, CallExpr, EPX(stmt, IdExpr, "_dlsym"), move(args))));
    // f = function[ARGS](ptr)
    args.clear();
    args.push_back(stmt->ret ? transform(stmt->ret)
                             : EPX(stmt, IdExpr, "void"));
    for (auto &a : stmt->args) {
      args.push_back(transform(a.type));
    }
    stmts.push_back(SP(AssignStmt, EPX(stmt, IdExpr, "f"),
                       EPX(stmt, CallExpr,
                           EPX(stmt, IndexExpr, EPX(stmt, IdExpr, "function"),
                               EPX(stmt, TupleExpr, move(args))),
                           EPX(stmt, IdExpr, "ptr"))));
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
      args.push_back(
          EPX(stmt, IdExpr, a.name != "" ? a.name : format("$a{}", ia++)));
    }
    // return f(args)
    auto call = EPX(stmt, CallExpr, EPX(stmt, IdExpr, "f"), move(args));
    if (!isVoid) {
      stmts.push_back(SP(ReturnStmt, move(call)));
    } else {
      stmts.push_back(SP(ExprStmt, move(call)));
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
           SP(SuiteStmt, move(stmts)), vector<string>());
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
    auto call =
        EPX(stmt, CallExpr, // _py_import(LIB)[WHAT].call ( ...
            EPX(stmt, DotExpr,
                EPX(stmt, IndexExpr,
                    EPX(stmt, CallExpr, EPX(stmt, IdExpr, "_py_import"),
                        EPX(stmt, StringExpr, from)),
                    EPX(stmt, StringExpr, stmt->name.first)),
                "call"),
            EPX(stmt, CallExpr, // ... x.__to_py__() )
                EPX(stmt, DotExpr, EPX(stmt, IdExpr, "x"), "__to_py__")));
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
      stmts.push_back(
          SP(ReturnStmt,
             EPX(stmt, CallExpr,
                 EPX(stmt, DotExpr, transform(stmt->ret), "__from_py__"),
                 move(call))));
    } else {
      stmts.push_back(SP(ExprStmt, move(call)));
    }
    vector<Param> params;
    params.push_back({"x", nullptr, nullptr});
    RETURN(FunctionStmt,
           stmt->name.second != "" ? stmt->name.second : stmt->name.first,
           transform(stmt->ret), vector<string>(), move(params),
           SP(SuiteStmt, move(stmts)), vector<string>{"pyhandle"});
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
  result = SP(ThrowStmt, transform(stmt->expr));
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
      argTypes.push_back(
          make_pair(a.name, a.type ? t->getType() : ctx.addUnbound(false)));
      args.push_back({a.name, move(t), transform(a.deflt)});
    }
    auto ret =
        stmt->ret ? transform(stmt->ret)->getType() : ctx.addUnbound(false);
    ctx.decreaseLevel();
    ctx.popBlock();

    auto type = make_shared<FuncType>(stmt->name, canonicalName, genericTypes,
                                      argTypes, ret);
    auto t = type->generalize(ctx.level);
    DBG("* [function] {} :- {}", ctx.prefix + stmt->name, *t);
    ctx.add(ctx.prefix + stmt->name, t);

    auto fp = SP(FunctionStmt, stmt->name, nullptr, stmt->generics, move(args),
                 stmt->suite, stmt->attributes);
    ctx.funcASTs[canonicalName] = make_pair(t, move(fp));
  }
  result = SP(FunctionStmt, stmt->name, nullptr, vector<string>(),
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
  result = SP(ClassStmt, stmt->isRecord, stmt->name, vector<string>(),
              vector<Param>(), SP(SuiteStmt, move(suite)));
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
  RETURN(AssignStmt, transform(stmt->lhs),
         EPX(stmt, BinaryExpr, transform(stmt->lhs), stmt->op,
             transform(stmt->rhs), true),
         nullptr, true);
}

// TODO
void TransformStmtVisitor::visit(const YieldFromStmt *stmt) {
  auto var = getTemporaryVar("yield");
  vector<StmtPtr> stmts;
  stmts.push_back(SP(YieldStmt, EPX(stmt, IdExpr, var)));
  RETURN(ForStmt, EPX(stmt, IdExpr, var), transform(stmt->expr),
         SP(SuiteStmt, move(stmts)));
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
    string var = stmt->items[i].second == "" ? getTemporaryVar("with")
                                             : stmt->items[i].second;
    internals.push_back(SP(AssignStmt, EPX(stmt, IdExpr, var),
                           transform(stmt->items[i].first)));
    internals.push_back(
        SP(ExprStmt,
           EPX(stmt, CallExpr,
               EPX(stmt, DotExpr, EPX(stmt, IdExpr, var), "__enter__"))));
    internals.push_back(
        SP(TryStmt,
           content.size() ? transform(SP(SuiteStmt, move(content)))
                          : transform(stmt->suite),
           vector<TryStmt::Catch>(),
           transform(SP(ExprStmt, EPX(stmt, CallExpr,
                                      EPX(stmt, DotExpr, EPX(stmt, IdExpr, var),
                                          "__exit__"))))));
    content = move(internals);
  }
  vector<IfStmt::If> ifs;
  ifs.push_back({EPX(stmt, BoolExpr, true), SP(SuiteStmt, move(content))});
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
      SP(ExprStmt, EPX(stmt, CallExpr, EPX(stmt, IdExpr, "_py_exec"),
                       EPX(stmt, StringExpr, code))));
  // from __main__ pyimport foo () -> ret
  stmts.push_back(transform(SP(ExternImportStmt, make_pair(stmt->name, ""),
                               EPX(stmt, IdExpr, "__main__"),
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
