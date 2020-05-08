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

namespace seq {
namespace ast {

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

TransformVisitor::TransformVisitor(shared_ptr<TypeContext> ctx,
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
    error(expr, "expected a type, got {}", *e);
  return e;
}

/*************************************************************************************/

void TransformVisitor::visit(const NoneExpr *expr) {
  resultExpr = expr->clone();
  if (!expr->getType())
    resultExpr->setType(ctx->addUnbound(getSrcInfo()));
}

void TransformVisitor::visit(const BoolExpr *expr) {
  resultExpr = expr->clone();
  if (!expr->getType())
    resultExpr->setType(T<LinkType>(ctx->findInternal("bool")));
}

void TransformVisitor::visit(const IntExpr *expr) {
  resultExpr = expr->clone();
  if (!expr->getType())
    resultExpr->setType(T<LinkType>(ctx->findInternal("int")));
}

void TransformVisitor::visit(const FloatExpr *expr) {
  resultExpr = expr->clone();
  if (!expr->getType())
    resultExpr->setType(T<LinkType>(ctx->findInternal("float")));
}

void TransformVisitor::visit(const StringExpr *expr) {
  resultExpr = expr->clone();
  if (!expr->getType())
    resultExpr->setType(T<LinkType>(ctx->findInternal("str")));
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

shared_ptr<TContextItem>
TransformVisitor::processIdentifier(shared_ptr<TypeContext> tctx,
                                    const string &id) {
  auto val = tctx->find(id);
  if (!val || val->isImport() ||
      (val->isVar() && !val->isGlobal() && val->getBase() != ctx->getBase()))
    error("identifier '{}' not found", id);
  return val;
}

void TransformVisitor::visit(const IdExpr *expr) {
  resultExpr = expr->clone();
  if (!expr->getType()) {
    auto val = processIdentifier(ctx, expr->value);
    if (val->isType())
      resultExpr->markType();
    resultExpr->setType(ctx->instantiate(getSrcInfo(), val->getType()));
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
  e->setType(forceUnify(
      expr, T<LinkType>(T<ClassType>("tuple", true,
                                     vector<pair<int, TypePtr>>(), args))));
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
TransformVisitor::CaptureVisitor::CaptureVisitor(shared_ptr<TypeContext> ctx)
    : ctx(ctx) {}

void TransformVisitor::CaptureVisitor::visit(const IdExpr *expr) {
  auto val = ctx->find(expr->value);
  if (!val || val->isImport())
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
  e->setType(forceUnify(expr, e->eif->getType()));
  resultExpr = move(e);
}

// Transformed
void TransformVisitor::visit(const UnaryExpr *expr) {
  if (expr->op == "!") { // Special case
    auto e = N<UnaryExpr>(expr->op, makeBoolExpr(expr->expr));
    e->setType(forceUnify(expr, ctx->findInternal("bool")));
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
  auto magics = unordered_map<string, string>{
      {"+", "add"},     {"-", "sub"},  {"*", "mul"},     {"**", "pow"},
      {"/", "truediv"}, {"//", "div"}, {"@", "mathmul"}, {"%", "mod"},
      {"<", "lt"},      {"<=", "le"},  {">", "gt"},      {">=", "ge"},
      {"==", "eq"},     {"!=", "ne"},  {"<<", "lshift"}, {">>", "rshift"},
      {"&", "and"},     {"|", "or"},   {"^", "xor"}};
  if (expr->op == "&&" || expr->op == "||") { // Special case
    auto e = N<BinaryExpr>(makeBoolExpr(expr->lexpr), expr->op,
                           makeBoolExpr(expr->rexpr));
    e->setType(forceUnify(expr, ctx->findInternal("bool")));
    resultExpr = move(e);
  } else if (expr->op == "is") {
    auto e =
        N<BinaryExpr>(transform(expr->lexpr), expr->op, transform(expr->rexpr));
    e->setType(forceUnify(expr, ctx->findInternal("bool")));
    resultExpr = move(e);
  } else if (expr->op == "is not") {
    resultExpr = transform(N<UnaryExpr>(
        "!", N<BinaryExpr>(expr->lexpr->clone(), "is", expr->rexpr->clone())));
  } else if (expr->op == "not in") {
    resultExpr = transform(N<UnaryExpr>(
        "!", N<BinaryExpr>(expr->lexpr->clone(), "in", expr->rexpr->clone())));
  } else if (expr->op == "in") {
    resultExpr =
        transform(N<CallExpr>(N<DotExpr>(expr->lexpr->clone(), "__contains__"),
                              expr->rexpr->clone()));
  } else {
    auto le = transform(expr->lexpr);
    auto re = transform(expr->rexpr);
    if (getUnbound(le->getType()) || getUnbound(re->getType())) {
      resultExpr = N<BinaryExpr>(move(le), expr->op, move(re));
      resultExpr->setType(ctx->addUnbound(getSrcInfo()));
    } else {
      auto mi = magics.find(expr->op);
      if (mi == magics.end())
        error(expr, "invalid binary operator '{}'", expr->op);
      auto magic = mi->second;
      auto lc = getClass(le->getType()), rc = getClass(re->getType());
      assert(lc && rc);
      // TODO select proper magic
      if (ctx->getRealizations()->findMethod(lc->name, magic
                                             /*{re->getType()}*/)) {
        if (expr->inPlace && ctx->getRealizations()->findMethod(
                                 lc->name, "i" + magic /*{re->getType()}*/))
          magic = "i" + magic;
      } else if (ctx->getRealizations()->findMethod(rc->name, magic
                                                    /*{le->getType()}*/)) {
        magic = "r" + magic;
      }
      magic = format("__{}__", magic);
      resultExpr =
          transform(N<CallExpr>(N<DotExpr>(move(le), magic), move(re)));
    }
  }
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
        forceUnify(f->generics[i].second, generics[i]);
    };
    // Instantiate the type
    // TODO: special cases (Kmer, Int, UInt)
    if (auto f = getFunction(e->getType())) {
      if (f->name == "") {
        assert(!f->ret && !f->args.size());
        f->ret = generics[0];
        for (int i = 1; i < generics.size(); i++)
          f->args.push_back({"", generics[i]});
      } else {
        uf(f);
      }
    } else if (auto g = getClass(e->getType())) {
      if (g->name == "tuple") {
        assert(!g->args.size());
        for (auto &i : generics)
          g->args.push_back({"", i});
      } else {
        uf(g);
      }
    } else
      assert(false);

    auto t = e->getType();
    resultExpr = N<TypeOfExpr>(move(e));
    resultExpr->markType();
    resultExpr->setType(forceUnify(expr, t));
  } else {
    if (auto c = getClass(e->getType()))
      if (c->name == "tuple") {
        auto i = transform(expr->index);
        if (auto ii = CAST(i, IntExpr)) {
          auto idx = std::stol(ii->value);
          if (idx < 0 || idx >= c->args.size())
            error(i, "invalid tuple index");
          resultExpr = N<IndexExpr>(move(e), move(i));
          resultExpr->setType(forceUnify(expr, c->args[idx].second));
          return;
        }
      }
    resultExpr = transform(
        N<CallExpr>(N<DotExpr>(move(e), "__getitem__"), expr->index->clone()));
  }
}

void TransformVisitor::visit(const CallExpr *expr) {
  // TODO: argument name resolution should come here!
  // TODO: handle the case when a member is of type function[...]
  /// TODO: wrap pyobj arguments in tuple

  ExprPtr e = nullptr;
  vector<CallExpr::Arg> args;
  // Intercept obj.foo() calls and transform obj.foo(...) to foo(obj, ...)
  if (auto d = CAST(expr->expr, DotExpr)) {
    auto dotlhs = transform(d->expr, true);
    if (!dotlhs->isType()) {
      if (auto c = getClass(dotlhs->getType())) { // Find appropriate function!
        if (auto m = ctx->getRealizations()->findMethod(c->name, d->member)) {
          args.push_back({"", move(dotlhs)});
          e = N<IdExpr>(m->name);
          e->setType(ctx->instantiate(getSrcInfo(), m, c->generics));
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
  forceUnify(expr->expr.get(), e->getType());

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
    vector<CallExpr::Arg> reorderedArgs;
    vector<int> positions;
    unordered_map<string, ExprPtr> namedArgs;
    bool namesStarted = false;
    for (int i = 0; i < args.size(); i++) {
      if (args[i].name == "" && namesStarted)
        error(expr, "unexpected unnamed argument after a named argument");
      namesStarted |= args[i].name != "";
      if (args[i].name == "")
        reorderedArgs.push_back({"", move(args[i].value)});
      else if (namedArgs.find(args[i].name) == namedArgs.end())
        namedArgs[args[i].name] = move(args[i].value);
      else
        error(expr, "named argument {} repeated", args[i].name);
    }
    if (reorderedArgs.size() + namedArgs.size() > t->countPartials()) {
      if (namedArgs.size() == 0 &&
          reorderedArgs.size() == t->countPartials() + 1 &&
          CAST(reorderedArgs.back().value, EllipsisExpr)) {
        reorderedArgs.pop_back();
        t->partialArgs.back() = 1;
      }
      error(expr, "too many arguments for {}", *t);
    }
    for (int i = 0, added = 0; i < t->args.size(); i++) {
      if (!t->partialArgs[i])
        continue;
      if (added >= reorderedArgs.size()) {
        auto it = namedArgs.find(t->args[i].first);
        if (it != namedArgs.end()) {
          reorderedArgs.push_back({"", move(it->second)});
          namedArgs.erase(it);
        } /* TODO: else if (auto s = getDefault(t, t->args[i].first)) { // default=
          // TODO: multi-threaded safety
          reorderedArgs.push_back({"", transform(N<IdExpr>(s))});
        } */ else {
          reorderedArgs.push_back({"", transform(N<EllipsisExpr>())});
        }
      }
      positions.push_back(i);
      added++;
    }
    for (auto &i : namedArgs)
      error(i.second, "unknown parameter {}", i.first);

    for (int i = 0; i < reorderedArgs.size(); i++) {
      if (!CAST(reorderedArgs[i].value, EllipsisExpr))
        t->partialArgs[positions[i]] = 0;
      forceUnify(t->args[positions[i]].second,
                 reorderedArgs[i].value->getType());
    }
    // special case: calling partial void f(...)()
    if (!args.size() && t->countPartials() == 1 && t->partialArgs.back())
      t->partialArgs.back() = 0;
    if (t->canRealize())
      t = realize(t).type;
    resultExpr = N<CallExpr>(move(e), move(reorderedArgs));
    resultExpr->setType(forceUnify(expr, make_shared<LinkType>(t->ret)));
  } else { // will be handled later on
    resultExpr = N<CallExpr>(move(e), move(args));
    resultExpr->setType(expr->getType() ? expr->getType()
                                        : ctx->addUnbound(getSrcInfo()));
  }
}

void TransformVisitor::visit(const DotExpr *expr) {
  // Handle import chains separately
  const ExprPtr *e = &(expr->expr);
  deque<string> chain;
  while (auto d = dynamic_cast<DotExpr *>(e->get())) {
    chain.push_front(d->member);
    e = &(d->expr);
  }
  if (auto d = dynamic_cast<IdExpr *>(e->get())) {
    chain.push_front(d->value);
    auto s = join(chain, "/");
    auto val = ctx->find(s);
    if (val && val->isImport()) {
      resultExpr = N<DotExpr>(N<IdExpr>(s), expr->member);
      auto ival = processIdentifier(
          ctx->getImports()->getImport(val->getBase()), expr->member);
      if (ival->isType())
        resultExpr->markType();
      resultExpr->setType(
          forceUnify(expr, ctx->instantiate(getSrcInfo(), ival->getType())));
      return;
    }
  }

  auto lhs = transform(expr->expr, true);
  TypePtr typ = nullptr;
  if (getUnbound(lhs->getType())) {
    typ = expr->getType() ? expr->getType() : ctx->addUnbound(getSrcInfo());
  } else if (auto c = getClass(lhs->getType())) {
    if (auto m = ctx->getRealizations()->findMethod(c->name, expr->member)) {
      if (lhs->isType()) {
        resultExpr = N<IdExpr>(m->name);
        resultExpr->setType(ctx->instantiate(getSrcInfo(), m, c->generics));
        return;
      } else
        error(expr, "cannot handle partials yet");
      // TODO: for now, this method cannot handle obj.method expression
      // (CallExpr does that for obj.method() expressions).
    } else if (auto mm =
                   ctx->getRealizations()->findMember(c->name, expr->member)) {
      typ = ctx->instantiate(getSrcInfo(), mm, c->generics);
    } else {
      error(expr, "cannot find '{}' in {}", expr->member, *lhs->getType());
    }
  } else {
    error(expr, "cannot search for '{}' in {}", expr->member, *lhs->getType());
  }
  resultExpr = N<DotExpr>(move(lhs), expr->member);
  resultExpr->setType(forceUnify(expr, typ));
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
  resultExpr->setType(nullptr);
}

// Should get transformed by other functions
void TransformVisitor::visit(const TypeOfExpr *expr) {
  resultExpr = N<TypeOfExpr>(transform(expr->expr, true));
  resultExpr->markType();
  resultExpr->setType(forceUnify(expr, expr->expr->getType()));
}

void TransformVisitor::visit(const PtrExpr *expr) {
  auto param = transform(expr->expr);
  auto t = ctx->instantiateGeneric(expr->getSrcInfo(), ctx->findInternal("ptr"),
                                   {param->getType()});
  resultExpr = N<PtrExpr>(move(param));
  resultExpr->setType(forceUnify(expr, t));
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

} // namespace ast
} // namespace seq
