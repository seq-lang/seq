/**
 * TODO : Redo error messages (right now they are awful)
 */

#include "util/fmt/format.h"
#include <deque>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/ast/ast/stmt.h"
#include "parser/ast/transform/transform.h"
#include "parser/ast/transform/transform_ctx.h"
#include "parser/ast/types.h"
#include "parser/common.h"
#include "parser/ocaml.h"

using fmt::format;
using std::deque;
using std::dynamic_pointer_cast;
using std::function;
using std::get;
using std::move;
using std::ostream;
using std::pair;
using std::stack;
using std::static_pointer_cast;

namespace seq {
namespace ast {

using namespace types;

TransformVisitor::TransformVisitor(shared_ptr<TransformContext> ctx,
                                   shared_ptr<vector<StmtPtr>> stmts)
    : ctx(move(ctx)) {
  prependStmts = stmts ? move(stmts) : make_shared<vector<StmtPtr>>();
}

StmtPtr TransformVisitor::apply(shared_ptr<Cache> cache, StmtPtr s,
                                const string &file) {
  auto suite = make_unique<SuiteStmt>();
  suite->stmts.push_back(make_unique<SuiteStmt>());
  auto *preamble = (SuiteStmt *)(suite->stmts[0].get());

  if (cache->imports.find("") == cache->imports.end()) {
    auto stdlib = make_shared<TransformContext>("", cache);
    auto stdlibPath = stdlib->findFile("internal", "", true);
    if (stdlibPath.empty())
      ast::error("cannot load standard library");
    stdlib->setFilename(stdlibPath);
    cache->imports[""] = {stdlibPath, stdlib};

    // Add preamble for core types and variardic stubs
    for (auto &name : {"void", "bool", "byte", "int", "float"}) {
      auto canonical = stdlib->generateCanonicalName(name);
      stdlib->add(TransformItem::Type, name, canonical, true);
      cache->asts[canonical] =
          make_unique<ClassStmt>(canonical, vector<Param>(), vector<Param>(), nullptr,
                                 vector<string>{"internal", "tuple"});
      preamble->stmts.push_back(clone(cache->asts[canonical]));
    }
    for (auto &name : vector<string>{"Ptr", "Generator", "Optional", "Int", "UInt"}) {
      auto canonical = stdlib->generateCanonicalName(name);
      stdlib->add(TransformItem::Type, name, canonical, true);
      vector<Param> generics;
      generics.emplace_back(Param{
          string(name) == "Int" || string(name) == "UInt" ? "N" : "T",
          string(name) == "Int" || string(name) == "UInt" ? make_unique<IdExpr>(".int")
                                                          : nullptr,
          nullptr});
      auto c = make_unique<ClassStmt>(canonical, move(generics), vector<Param>(),
                                      nullptr, vector<string>{"internal", "tuple"});
      if (name == "Generator")
        c->attributes["trait"] = "";
      preamble->stmts.push_back(clone(c));
      cache->asts[canonical] = move(c);
    }

    StmtPtr stmts = nullptr;
    // stdlib->setFlag("internal");
    assert(stdlibPath.substr(stdlibPath.size() - 12) == "__init__.seq");
    // auto internal = stdlibPath.substr(0, stdlibPath.size() - 12) +
    // "__internal__.seq"; stdlib->setFilename(internal);
    // Define str and pyobj before everything to support Function and Tuple definitions
    auto code = "@internal\n@tuple\nclass pyobj:\n  p: Ptr[byte]\n"
                "@internal\n@tuple\nclass str:\n  len: int\n  ptr: Ptr[byte]\n";
    preamble->stmts.push_back(
        TransformVisitor(stdlib).transform(parseCode(stdlibPath, code)));
    // Load __internal__
    // stmts = parseFile(internal);
    // suite->stmts.push_back(TransformVisitor(stdlib).transform(stmts));
    // stdlib->unsetFlag("internal");
    // Load stdlib
    stdlib->setFilename(stdlibPath);
    stmts = parseFile(stdlibPath);
    suite->stmts.push_back(TransformVisitor(stdlib).transform(stmts));

    stmts = make_unique<AssignStmt>(make_unique<IdExpr>("__argv__"), nullptr,
                                    make_unique<IndexExpr>(make_unique<IdExpr>("Array"),
                                                           make_unique<IdExpr>("str")));
    suite->stmts.push_back(TransformVisitor(stdlib).transform(stmts));
  }
  auto ctx = static_pointer_cast<TransformContext>(cache->imports[""].ctx);
  ctx->setFilename(file);
  auto stmts = TransformVisitor(ctx).transform(s);

  preamble->stmts.push_back(clone(cache->asts[".Function.1"])); // main dependency
  for (auto &v : cache->variardics)
    if (v != ".Function.1")
      preamble->stmts.push_back(clone(cache->asts["." + v]));
  suite->stmts.push_back(move(stmts));
  return move(suite);
}

ExprPtr TransformVisitor::transform(const ExprPtr &expr) {
  return transform(expr, false);
}

ExprPtr TransformVisitor::transform(const ExprPtr &expr, bool allowTypes) {
  if (!expr)
    return nullptr;
  TransformVisitor v(ctx, prependStmts);
  v.setSrcInfo(expr->getSrcInfo());
  expr->accept(v);
  if (!allowTypes && v.resultExpr && v.resultExpr->isType())
    error("unexpected type expression");
  return move(v.resultExpr);
}

ExprPtr TransformVisitor::transformType(const ExprPtr &expr) {
  auto e = transform(expr, true);
  if (e && !e->isType())
    error("expected type expression");
  return e;
}

StmtPtr TransformVisitor::transform(const StmtPtr &stmt) {
  if (!stmt)
    return nullptr;

  TransformVisitor v(ctx);
  v.setSrcInfo(stmt->getSrcInfo());
  stmt->accept(v);
  if (!v.prependStmts->empty()) {
    if (v.resultStmt)
      v.prependStmts->push_back(move(v.resultStmt));
    v.resultStmt = N<SuiteStmt>(move(*v.prependStmts));
  }
  return move(v.resultStmt);
}

PatternPtr TransformVisitor::transform(const PatternPtr &pat) {
  if (!pat)
    return nullptr;
  TransformVisitor v(ctx, prependStmts);
  v.setSrcInfo(pat->getSrcInfo());
  pat->accept(v);
  return move(v.resultPattern);
}

void TransformVisitor::defaultVisit(const Expr *e) { resultExpr = e->clone(); }

void TransformVisitor::defaultVisit(const Stmt *s) { resultStmt = s->clone(); }

void TransformVisitor::defaultVisit(const Pattern *p) { resultPattern = p->clone(); }

/*************************************************************************************/

void TransformVisitor::visit(const NoneExpr *expr) {
  resultExpr = transform(N<CallExpr>(N<IdExpr>(".Optional")));
}

void TransformVisitor::visit(const IntExpr *expr) {
  resultExpr = transformInt(expr->value, expr->suffix);
}

void TransformVisitor::visit(const StringExpr *expr) {
  if (expr->prefix == "f") {
    resultExpr = parseFString(expr->value);
  } else if (!expr->prefix.empty()) {
    resultExpr = transform(
        N<CallExpr>(N<IndexExpr>(N<DotExpr>(N<IdExpr>(".str"),
                                            format("__prefix_{}__", expr->prefix)),
                                 N<IntExpr>(expr->value.size())),
                    N<StringExpr>(expr->value)));
  } else {
    resultExpr = expr->clone();
  }
}

void TransformVisitor::visit(const IdExpr *expr) {
  auto val = ctx->find(expr->value);
  if (!val) {
    // ctx->dump();
    error("identifier '{}' not found", expr->value);
  }
  if (val->isVar()) {
    if (ctx->getBase() != val->getBase() && !val->isGlobal()) {
      if (!ctx->captures.empty())
        ctx->captures.back().insert(expr->value);
      else
        error("cannot access non-toplevel variable '{}'", expr->value);
    }
  }

  resultExpr = N<IdExpr>(val->canonicalName.empty() ? expr->value : val->canonicalName);
  if (val->isType() && !val->isStatic())
    resultExpr->markType();

  for (int i = int(ctx->bases.size()) - 1; i >= 0; i--)
    if (ctx->bases[i].name == val->getBase()) {
      for (int j = i + 1; j < ctx->bases.size(); j++) {
        ctx->bases[j].parent = std::max(i, ctx->bases[j].parent);
        assert(ctx->bases[j].parent < j);
      }
      return;
    }
  seqassert(val->getBase().empty(), "a variable '{}' has invalid base {}", expr->value,
            val->getBase());
}

void TransformVisitor::visit(const StarExpr *expr) {
  resultExpr = transform(N<CallExpr>(N<IdExpr>(".List"), clone(expr->what)));
}

void TransformVisitor::visit(const TupleExpr *expr) {
  auto name = generateTupleStub(expr->items.size());
  resultExpr = transform(
      N<CallExpr>(N<DotExpr>(N<IdExpr>(name), "__new__"), clone(expr->items)));
}

void TransformVisitor::visit(const ListExpr *expr) {
  vector<StmtPtr> stmts;
  ExprPtr var = N<IdExpr>(getTemporaryVar("list"));
  stmts.push_back(transform(N<AssignStmt>(
      clone(var),
      N<CallExpr>(N<IdExpr>(".List"),
                  !expr->items.empty() ? N<IntExpr>(expr->items.size()) : nullptr))));
  for (auto &it : expr->items)
    stmts.push_back(transform(
        N<ExprStmt>(N<CallExpr>(N<DotExpr>(clone(var), "append"), clone(it)))));
  resultExpr = N<StmtExpr>(move(stmts), transform(var));
}

void TransformVisitor::visit(const SetExpr *expr) {
  vector<StmtPtr> stmts;
  ExprPtr var = N<IdExpr>(getTemporaryVar("set"));
  stmts.push_back(transform(N<AssignStmt>(clone(var), N<CallExpr>(N<IdExpr>(".Set")))));
  for (auto &it : expr->items)
    stmts.push_back(
        transform(N<ExprStmt>(N<CallExpr>(N<DotExpr>(clone(var), "add"), clone(it)))));
  resultExpr = N<StmtExpr>(move(stmts), transform(var));
}

void TransformVisitor::visit(const DictExpr *expr) {
  vector<StmtPtr> stmts;
  ExprPtr var = N<IdExpr>(getTemporaryVar("dict"));
  stmts.push_back(
      transform(N<AssignStmt>(clone(var), N<CallExpr>(N<IdExpr>(".Dict")))));
  for (auto &it : expr->items)
    stmts.push_back(transform(N<ExprStmt>(N<CallExpr>(
        N<DotExpr>(clone(var), "__setitem__"), clone(it.key), clone(it.value)))));
  resultExpr = N<StmtExpr>(move(stmts), transform(var));
}

void TransformVisitor::visit(const GeneratorExpr *expr) {
  SuiteStmt *prev;
  auto suite = getGeneratorBlock(expr->loops, prev);

  vector<StmtPtr> stmts;
  ExprPtr var = N<IdExpr>(getTemporaryVar("gen"));
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

void TransformVisitor::visit(const DictGeneratorExpr *expr) {
  SuiteStmt *prev;
  auto suite = getGeneratorBlock(expr->loops, prev);

  vector<StmtPtr> stmts;
  ExprPtr var = N<IdExpr>(getTemporaryVar("gen"));
  stmts.push_back(
      transform(N<AssignStmt>(clone(var), N<CallExpr>(N<IdExpr>(".Dict")))));
  prev->stmts.push_back(N<ExprStmt>(N<CallExpr>(N<DotExpr>(clone(var), "__setitem__"),
                                                clone(expr->key), clone(expr->expr))));
  stmts.push_back(transform(suite));
  resultExpr = N<StmtExpr>(move(stmts), transform(var));
}

void TransformVisitor::visit(const IfExpr *expr) {
  resultExpr =
      N<IfExpr>(transform(N<CallExpr>(N<DotExpr>(clone(expr->cond), "__bool__"))),
                transform(expr->ifexpr), transform(expr->elsexpr));
}

void TransformVisitor::visit(const UnaryExpr *expr) {
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

void TransformVisitor::visit(const BinaryExpr *expr) {
  if (expr->op == "&&" || expr->op == "||") {
    resultExpr = N<BinaryExpr>(
        transform(N<CallExpr>(N<DotExpr>(clone(expr->lexpr), "__bool__"))), expr->op,
        transform(N<CallExpr>(N<DotExpr>(clone(expr->rexpr), "__bool__"))));
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
    auto le = CAST(expr->lexpr, NoneExpr) ? clone(expr->lexpr) : transform(expr->lexpr);
    auto re = CAST(expr->rexpr, NoneExpr) ? clone(expr->rexpr) : transform(expr->rexpr);
    if (CAST(expr->lexpr, NoneExpr) && CAST(expr->rexpr, NoneExpr))
      resultExpr = N<BoolExpr>(true);
    else if (CAST(expr->lexpr, NoneExpr))
      resultExpr = N<BinaryExpr>(move(re), expr->op, move(le));
    else
      resultExpr = N<BinaryExpr>(move(le), expr->op, move(re));
  } else {
    resultExpr =
        N<BinaryExpr>(transform(expr->lexpr), expr->op, transform(expr->rexpr));
  }
}

void TransformVisitor::visit(const PipeExpr *expr) {
  vector<PipeExpr::Pipe> p;
  for (auto &i : expr->items)
    p.push_back({i.op, transform(i.expr)});
  resultExpr = N<PipeExpr>(move(p));
}

void TransformVisitor::visit(const IndexExpr *expr) {
  ExprPtr e = nullptr;
  if (expr->expr->isId("tuple") || expr->expr->isId("Tuple")) {
    auto t = expr->index->getTuple();
    auto name = generateTupleStub(t ? t->items.size() : 1);
    e = transformType(N<IdExpr>(name));
    e->markType();
  } else if (expr->expr->isId("function") || expr->expr->isId("Function")) {
    auto t = expr->index->getTuple();
    auto name = generateFunctionStub(t ? t->items.size() : 1);
    e = transformType(N<IdExpr>(name));
    e->markType();
  } else {
    e = transform(expr->expr, true);
  }
  vector<ExprPtr> it;
  if (auto t = CAST(expr->index, TupleExpr))
    for (auto &i : t->items)
      it.push_back(transformGenericExpr(i));
  else
    it.push_back(transformGenericExpr(expr->index));
  bool allTypes = true;
  bool hasRealTypes = false;
  for (auto &i : it) {
    bool isType = i->isType() || CAST(i, StaticExpr);
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
  } else { // for some functions we might need to delay the instantiation because of
           // staticExprs...
    resultExpr =
        N<IndexExpr>(move(e), it.size() == 1 ? move(it[0]) : N<TupleExpr>(move(it)));
  }
}

void TransformVisitor::visit(const CallExpr *expr) {
  if (expr->expr->isId("__ptr__")) {
    if (expr->args.size() == 1) {
      if (auto id = CAST(expr->args[0].value, IdExpr)) {
        auto v = ctx->find(id->value);
        if (v && v->isVar()) {
          resultExpr = N<PtrExpr>(transform(expr->args[0].value));
          return;
        }
      }
    }
    error("__ptr__ requires a variable");
  }
  if (auto ix = CAST(expr->expr, IndexExpr))
    if (ix->expr->isId("__array__")) {
      if (expr->args.size() != 1)
        error("__array__ requires only size argument");
      resultExpr =
          N<StackAllocExpr>(transformType(ix->index), transform(expr->args[0].value));
      return;
    }
  generateTupleStub(expr->args.size());
  vector<CallExpr::Arg> args;
  for (auto &i : expr->args)
    args.push_back({i.name, transform(i.value)});
  resultExpr = N<CallExpr>(transform(expr->expr, true), move(args));
}

void TransformVisitor::visit(const DotExpr *expr) {
  const ExprPtr *e = &(expr->expr);
  deque<string> chain;
  while (auto d = dynamic_cast<DotExpr *>(e->get())) {
    chain.push_front(d->member);
    e = &(d->expr);
  }
  if (auto d = dynamic_cast<IdExpr *>(e->get())) {
    chain.push_front(d->value);
    auto s = join(chain, "/");
    if (!s.size() || s[0] != '/') {
      auto val = ctx->find(s);
      s = val && val->isImport() ? val->canonicalName : "";
    }
    if (s.size()) {
      auto ictx = ctx->cache->imports[s].ctx;
      auto ival = ictx->find(expr->member);
      if (!ival || !ival->isGlobal())
        error("identifier '{}' not found in {}", expr->member, s);
      seqassert(!ival->canonicalName.empty(), "'{}' in {} does not have canonical name",
                expr->member, s);
      resultExpr = N<IdExpr>(ival->canonicalName);
      if (ival->isType())
        resultExpr->markType();
      return;
    }
  }
  resultExpr = N<DotExpr>(transform(expr->expr, true), expr->member);
}

void TransformVisitor::visit(const SliceExpr *expr) {
  // Further transformed at typecheck stage  as we need raw SliceExpr for static tuple
  // indices
  resultExpr = N<SliceExpr>(transform(expr->start), transform(expr->stop),
                            transform(expr->step));
}

void TransformVisitor::visit(const TypeOfExpr *expr) {
  resultExpr = N<TypeOfExpr>(transform(expr->expr, true));
  resultExpr->markType();
}

void TransformVisitor::visit(const LambdaExpr *expr) {
  vector<StmtPtr> stmts;
  stmts.push_back(N<ReturnStmt>(clone(expr->expr)));
  auto c = makeAnonFn(move(stmts), expr->vars);
  auto cc = CAST(c, CallExpr);
  if (cc->args.size()) { // create partial call
    for (int i = 0; i < expr->vars.size(); i++)
      cc->args.insert(cc->args.begin(), {"", N<EllipsisExpr>()});
    resultExpr = transform(c);
  } else {
    resultExpr = move(cc->expr);
  }
}

void TransformVisitor::visit(const SuiteStmt *stmt) {
  vector<StmtPtr> r;
  if (stmt->ownBlock)
    ctx->addBlock();
  for (auto &s : stmt->stmts)
    if (auto t = transform(s))
      r.push_back(move(t));
  if (stmt->ownBlock)
    ctx->popBlock();
  resultStmt = N<SuiteStmt>(move(r), stmt->ownBlock);
}

void TransformVisitor::visit(const ContinueStmt *stmt) {
  if (ctx->loops.empty())
    error("continue outside of a loop");
  resultStmt = stmt->clone();
}

void TransformVisitor::visit(const BreakStmt *stmt) {
  if (ctx->loops.empty())
    error("break outside of a loop");
  if (!ctx->loops.back().empty()) {
    resultStmt = N<SuiteStmt>(
        transform(N<AssignStmt>(N<IdExpr>(ctx->loops.back()), N<BoolExpr>(false))),
        stmt->clone());
  } else {
    resultStmt = stmt->clone();
  }
}

void TransformVisitor::visit(const ExprStmt *stmt) {
  resultStmt = N<ExprStmt>(transform(stmt->expr));
}

void TransformVisitor::visit(const AssignStmt *stmt) {
  auto add = [&](const ExprPtr &lhs, const ExprPtr &rhs, const ExprPtr &type,
                 bool shadow, bool mustExist) -> StmtPtr {
    auto p = lhs.get();
    if (auto l = CAST(lhs, IndexExpr)) {
      vector<ExprPtr> args;
      args.push_back(clone(l->index));
      args.push_back(clone(rhs));
      return transform(
          Nx<ExprStmt>(p, Nx<CallExpr>(p, Nx<DotExpr>(p, clone(l->expr), "__setitem__"),
                                       move(args))));
    } else if (auto l = CAST(lhs, DotExpr)) {
      return Nx<AssignMemberStmt>(p, transform(l->expr), l->member, transform(rhs));
    } else if (auto l = CAST(lhs, IdExpr)) {
      auto s = Nx<AssignStmt>(p, clone(lhs), transform(rhs, true), transformType(type));
      if (!shadow && !s->type) {
        auto val = ctx->find(l->value);
        if (val && val->isVar()) {
          if (val->getBase() == ctx->getBase())
            return Nx<UpdateStmt>(p, transform(lhs), move(s->rhs));
          else if (mustExist)
            error("variable '{}' is not global", l->value);
        }
      }
      if (auto r = CAST(rhs, IdExpr)) { // simple rename?
        auto val = ctx->find(r->value);
        if (!val)
          error("cannot find '{}'", r->value);
        if (val->isType() || val->isFunc()) {
          ctx->add(l->value, val);
          return nullptr;
        }
      }
      auto canonical = ctx->isToplevel() ? ctx->generateCanonicalName(l->value) : "";
      if (!canonical.empty())
        s->lhs = Nx<IdExpr>(p, canonical);
      if (s->rhs && s->rhs->isType())
        ctx->add(TransformItem::Type, l->value, canonical, ctx->isToplevel());
      else
        /// TODO: all toplevel variables are global now!
        ctx->add(TransformItem::Var, l->value, canonical, ctx->isToplevel());
      return s;
    } else {
      error("invalid assignment");
      return nullptr;
    }
  };
  function<void(const ExprPtr &, const ExprPtr &, vector<StmtPtr> &, bool, bool)>
      process = [&](const ExprPtr &lhs, const ExprPtr &rhs, vector<StmtPtr> &stmts,
                    bool shadow, bool mustExist) -> void {
    vector<ExprPtr> lefts;
    if (auto l = CAST(lhs, TupleExpr)) {
      for (auto &i : l->items)
        lefts.push_back(clone(i));
    } else if (auto l = CAST(lhs, ListExpr)) {
      for (auto &i : l->items)
        lefts.push_back(clone(i));
    } else {
      stmts.push_back(add(lhs, rhs, nullptr, shadow, mustExist));
      return;
    }
    auto p = rhs.get();
    ExprPtr newRhs = nullptr;
    if (!CAST(rhs, IdExpr)) { // store any non-trivial expression
      auto var = getTemporaryVar("assign");
      newRhs = Nx<IdExpr>(p, var);
      stmts.push_back(add(newRhs, rhs, nullptr, shadow, mustExist));
    } else {
      newRhs = clone(rhs);
    }
    StarExpr *unpack = nullptr;
    int st = 0;
    for (; st < lefts.size(); st++) {
      if (auto u = CAST(lefts[st], StarExpr)) {
        unpack = u;
        break;
      }
      process(lefts[st], Nx<IndexExpr>(p, clone(newRhs), Nx<IntExpr>(p, st)), stmts,
              shadow, mustExist);
    }
    if (unpack) {
      process(unpack->what,
              Nx<IndexExpr>(p, clone(newRhs),
                            Nx<SliceExpr>(p, Nx<IntExpr>(p, st),
                                          lefts.size() == st + 1
                                              ? nullptr
                                              : Nx<IntExpr>(p, -lefts.size() + st + 1),
                                          nullptr)),
              stmts, shadow, mustExist);
      st += 1;
      for (; st < lefts.size(); st++) {
        if (CAST(lefts[st], StarExpr))
          error(lefts[st], "multiple unpack expressions found");
        process(lefts[st],
                Nx<IndexExpr>(p, clone(newRhs), Nx<IntExpr>(p, -lefts.size() + st)),
                stmts, shadow, mustExist);
      }
    }
  };

  vector<StmtPtr> stmts;
  if (stmt->rhs && stmt->rhs->getBinary() && stmt->rhs->getBinary()->inPlace) {
    seqassert(!stmt->type, "invalid AssignStmt {}", stmt->toString());
    process(stmt->lhs, stmt->rhs, stmts, false, true);
  } else if (stmt->type) {
    if (stmt->lhs->getId())
      stmts.push_back(add(stmt->lhs, stmt->rhs, stmt->type, true, false));
    else
      error("invalid type specifier");
  } else {
    process(stmt->lhs, stmt->rhs, stmts, false, false);
  }
  resultStmt = stmts.size() == 1 ? move(stmts[0]) : N<SuiteStmt>(move(stmts));
}

void TransformVisitor::visit(const DelStmt *stmt) {
  if (auto expr = CAST(stmt->expr, IndexExpr)) {
    resultStmt = N<ExprStmt>(transform(
        N<CallExpr>(N<DotExpr>(clone(expr->expr), "__delitem__"), clone(expr->index))));
  } else if (auto expr = CAST(stmt->expr, IdExpr)) {
    ctx->remove(expr->value);
  } else {
    error("expression cannot be deleted");
  }
}

void TransformVisitor::visit(const PrintStmt *stmt) {
  resultStmt = N<ExprStmt>(transform(N<CallExpr>(
      N<IdExpr>(".seq_print"), N<CallExpr>(N<DotExpr>(clone(stmt->expr), "__str__")))));
}

void TransformVisitor::visit(const ReturnStmt *stmt) {
  if (!ctx->getLevel() || ctx->bases.back().isType())
    error("expected function body");
  resultStmt = N<ReturnStmt>(transform(stmt->expr));
}

void TransformVisitor::visit(const YieldStmt *stmt) {
  if (!ctx->getLevel() || ctx->bases.back().isType())
    error("expected function body");
  resultStmt = N<YieldStmt>(transform(stmt->expr));
}

void TransformVisitor::visit(const YieldFromStmt *stmt) {
  auto var = getTemporaryVar("yield");
  resultStmt = transform(
      N<ForStmt>(N<IdExpr>(var), clone(stmt->expr), N<YieldStmt>(N<IdExpr>(var))));
}

void TransformVisitor::visit(const AssertStmt *stmt) {
  resultStmt = N<AssertStmt>(transform(stmt->expr));
}

void TransformVisitor::visit(const WhileStmt *stmt) {
  ExprPtr cond = N<CallExpr>(N<DotExpr>(clone(stmt->cond), "__bool__"));
  string breakVar;
  StmtPtr assign = nullptr;
  if (stmt->elseSuite) {
    breakVar = getTemporaryVar("no_break");
    assign = N<AssignStmt>(N<IdExpr>(breakVar), N<BoolExpr>(true));
  }
  ctx->loops.push_back(breakVar);
  StmtPtr whilestmt = N<WhileStmt>(transform(cond), transform(stmt->suite));
  ctx->loops.pop_back();
  if (stmt->elseSuite) {
    resultStmt =
        N<SuiteStmt>(move(assign), move(whilestmt),
                     N<IfStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(breakVar), "__bool__")),
                               transform(stmt->elseSuite)));
  } else {
    resultStmt = move(whilestmt);
  }
}

void TransformVisitor::visit(const ForStmt *stmt) {
  ExprPtr iter = N<CallExpr>(N<DotExpr>(clone(stmt->iter), "__iter__"));
  string breakVar;
  StmtPtr assign = nullptr, forstmt = nullptr;
  if (stmt->elseSuite) {
    breakVar = getTemporaryVar("no_break");
    assign = N<AssignStmt>(N<IdExpr>(breakVar), N<BoolExpr>(true));
  }
  ctx->loops.push_back(breakVar);
  ctx->addBlock();
  if (auto i = CAST(stmt->var, IdExpr)) {
    string varName = i->value;
    ctx->add(TransformItem::Var, varName);
    forstmt = N<ForStmt>(transform(stmt->var), transform(iter), transform(stmt->suite));
  } else {
    string varName = getTemporaryVar("for");
    ctx->add(TransformItem::Var, varName);
    auto var = N<IdExpr>(varName);
    vector<StmtPtr> stmts;
    stmts.push_back(N<AssignStmt>(clone(stmt->var), clone(var)));
    stmts.push_back(clone(stmt->suite));
    forstmt =
        N<ForStmt>(clone(var), transform(iter), transform(N<SuiteStmt>(move(stmts))));
  }
  ctx->popBlock();
  ctx->loops.pop_back();

  if (stmt->elseSuite) {
    resultStmt =
        N<SuiteStmt>(move(assign), move(forstmt),
                     N<IfStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(breakVar), "__bool__")),
                               transform(stmt->elseSuite)));
  } else {
    resultStmt = move(forstmt);
  }
}

void TransformVisitor::visit(const IfStmt *stmt) {
  vector<IfStmt::If> ifs;
  for (auto &i : stmt->ifs)
    ifs.push_back({transform(i.cond ? N<CallExpr>(N<DotExpr>(clone(i.cond), "__bool__"))
                                    : nullptr),
                   transform(i.suite)});
  resultStmt = N<IfStmt>(move(ifs));
}

void TransformVisitor::visit(const MatchStmt *stmt) {
  auto w = transform(stmt->what);
  vector<PatternPtr> patterns;
  vector<StmtPtr> cases;
  for (auto ci = 0; ci < stmt->cases.size(); ci++) {
    ctx->addBlock();
    if (auto p = CAST(stmt->patterns[ci], BoundPattern)) {
      ctx->add(TransformItem::Var, p->var);
      patterns.push_back(transform(p->pattern));
      cases.push_back(transform(stmt->cases[ci]));
    } else {
      patterns.push_back(transform(stmt->patterns[ci]));
      cases.push_back(transform(stmt->cases[ci]));
    }
    ctx->popBlock();
  }
  resultStmt = N<MatchStmt>(move(w), move(patterns), move(cases));
}

void TransformVisitor::visit(const TryStmt *stmt) {
  vector<TryStmt::Catch> catches;
  auto suite = transform(stmt->suite);
  for (auto &c : stmt->catches) {
    ctx->addBlock();
    if (c.var != "")
      ctx->add(TransformItem::Var, c.var);
    catches.push_back({c.var, transformType(c.exc), transform(c.suite)});
    ctx->popBlock();
  }
  resultStmt = N<TryStmt>(move(suite), move(catches), transform(stmt->finally));
}

void TransformVisitor::visit(const ThrowStmt *stmt) {
  resultStmt = N<ThrowStmt>(transform(stmt->expr));
}

void TransformVisitor::visit(const WithStmt *stmt) {
  assert(stmt->items.size());
  vector<StmtPtr> content;
  for (int i = stmt->items.size() - 1; i >= 0; i--) {
    vector<StmtPtr> internals;
    string var = stmt->vars[i] == "" ? getTemporaryVar("with") : stmt->vars[i];
    internals.push_back(N<AssignStmt>(N<IdExpr>(var), clone(stmt->items[i])));
    internals.push_back(
        N<ExprStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "__enter__"))));

    internals.push_back(N<TryStmt>(
        content.size() ? N<SuiteStmt>(move(content), true) : clone(stmt->suite),
        vector<TryStmt::Catch>{},
        N<SuiteStmt>(N<ExprStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "__exit__"))),
                     true)));
    content = move(internals);
  }
  resultStmt = transform(N<SuiteStmt>(move(content), true));
}

void TransformVisitor::visit(const GlobalStmt *stmt) {
  if (!ctx->bases.size() || ctx->bases.back().isType())
    error("'global' is only applicable within function blocks");
  auto val = ctx->find(stmt->var);
  if (!val || !val->isVar())
    error("identifier '{}' not found", stmt->var);
  if (val->getBase() != "")
    error("not a toplevel variable");
  val->global = true;
  seqassert(!val->canonicalName.empty(), "'{}' does not have a canonical name",
            stmt->var);
  ctx->add(TransformItem::Var, stmt->var, val->canonicalName);
}

void TransformVisitor::visit(const ImportStmt *stmt) {
  if (ctx->getLevel() && ctx->bases.back().isType())
    error("imports cannot be located within classes");

  if (stmt->from->isId("C")) {
    if (auto i = stmt->what->getId())
      resultStmt = parseCImport(i->value, stmt->args, stmt->ret, stmt->as);
    else if (auto d = stmt->what->getDot())
      resultStmt =
          parseDylibCImport(d->expr, d->member, stmt->args, stmt->ret, stmt->as);
    else
      error("invalid C import");
    return;
  } else if (stmt->from->isId("python")) {
    resultStmt = parsePythonImport(stmt->what, stmt->as);
    return;
  }

  vector<string> dirs;
  Expr *e = stmt->from.get();
  while (auto d = dynamic_cast<DotExpr *>(e)) {
    dirs.push_back(d->member);
    e = d->expr.get();
  }
  if (!e->getId() || !stmt->args.empty() || stmt->ret ||
      (stmt->what && !stmt->what->getId()))
    error("invalid import statement");
  dirs.push_back(e->getId()->value);
  // TODO: enforce locality with ".abc"
  seqassert(stmt->dots >= 0, "invalid ImportStmt.dots");
  for (int i = 0; i < stmt->dots - 1; i++)
    dirs.push_back("..");
  string path;
  for (int i = dirs.size() - 1; i >= 0; i--)
    path += dirs[i] + (i ? "/" : "");
  auto file = ctx->findFile(path, ctx->getFilename());
  if (file.empty())
    error("cannot locate import '{}'", stmt->from->toString());

  auto import = ctx->cache->imports.find(file);
  if (import == ctx->cache->imports.end()) {
    auto ictx = make_shared<TransformContext>(file, ctx->cache);
    import = ctx->cache->imports.insert({file, {file, ictx}}).first;
    StmtPtr s = parseFile(file);
    auto sn = TransformVisitor(ictx).transform(s);
    resultStmt = N<SuiteStmt>(move(sn), true);
  }

  if (!stmt->what) {
    ctx->add(TransformItem::Import, stmt->as.empty() ? path : stmt->as, file);
  } else if (stmt->what->isId("*")) {
    if (!stmt->as.empty())
      error("cannot rename star-import");
    for (auto &i : *(import->second.ctx))
      if (i.second.front().second->isGlobal())
        ctx->add(i.first, i.second.front().second);
  } else {
    auto i = stmt->what->getId();
    seqassert(i, "not a valid expression");
    auto c = import->second.ctx->find(i->value);
    if (!c || !c->isGlobal())
      error("symbol '{}' not found in {}", i->value, file);
    ctx->add(stmt->as.empty() ? i->value : stmt->as, c);
  }
}

void TransformVisitor::visit(const FunctionStmt *stmt) {
  if (in(stmt->attributes, "python")) {
    auto s = CAST(stmt->suite, ExprStmt);
    if (!s) {
      auto ss = CAST(stmt->suite, SuiteStmt);
      if (ss && ss->stmts.size() == 1)
        s = CAST(ss->stmts[0], ExprStmt);
    }
    if (!s || !CAST(s->expr, StringExpr))
      error("malformed external definition");
    string code = CAST(s->expr, StringExpr)->value;
    vector<string> args;
    for (auto &a : stmt->args)
      args.push_back(a.name);
    code = format("def {}({}):\n{}\n", stmt->name, fmt::join(args, ", "), code);
    resultStmt = transform(N<SuiteStmt>(
        N<ExprStmt>(N<CallExpr>(N<IdExpr>("_py_exec"), N<StringExpr>(code))),
        N<ImportStmt>(N<IdExpr>("python"), N<IdExpr>(stmt->name), clone_nop(stmt->args),
                      clone(stmt->ret))));
    return;
  }

  auto canonicalName = ctx->generateCanonicalName(stmt->name);
  bool isClassMember = ctx->getLevel() && ctx->bases.back().isType();

  // if (in(stmt->attributes, "llvm")) {
  //   auto s = CAST(stmt->suite, SuiteStmt);
  //   assert(s && s->stmts.size() == 1);
  //   auto sp = CAST(s->stmts[0], ExprStmt);
  //   seqassert(sp && CAST(sp->expr, StringExpr), "invalid llvm");

  //   vector<Param> args;
  //   for (int ia = 0; ia < stmt->args.size(); ia++) {
  //     auto &a = stmt->args[ia];
  //     auto typeAst = transformType(a.type);
  //     if (!typeAst && isClassMember && ia == 0 && a.name == "self")
  //       typeAst = transformType(ctx->bases[ctx->bases.size() - 1].ast);
  //     args.push_back(Param{a.name, move(typeAst), transform(a.deflt)});
  //   }
  //   resultStmt =
  //       parseCImport(stmt->name, args, stmt->ret, "", CAST(sp->expr, StringExpr));
  //   return;
  // }

  if (in(stmt->attributes, "builtin") && (ctx->getLevel() || isClassMember))
    error("builtins must be defined at the toplevel");

  generateFunctionStub(stmt->args.size() + 1);
  if (!isClassMember)
    ctx->add(TransformItem::Func, stmt->name, canonicalName, ctx->isToplevel());

  ctx->bases.push_back(TransformContext::Base{canonicalName});
  ctx->addBlock();
  vector<Param> newGenerics;
  for (auto &g : stmt->generics) {
    ctx->add(TransformItem::Type, g.name, "", false, true, g.type != nullptr);
    newGenerics.push_back(
        Param{g.name, transformType(g.type), transform(g.deflt, true)});
  }

  vector<Param> args;
  for (int ia = 0; ia < stmt->args.size(); ia++) {
    auto &a = stmt->args[ia];
    auto typeAst = transformType(a.type);
    if (!typeAst && isClassMember && ia == 0 && a.name == "self")
      typeAst = transformType(ctx->bases[ctx->bases.size() - 2].ast);
    args.push_back(Param{a.name, move(typeAst), transform(a.deflt)});
    ctx->add(TransformItem::Var, a.name);
  }
  if (!stmt->ret && in(stmt->attributes, "llvm"))
    error("LLVM functions must have a return type");
  auto ret = transformType(stmt->ret);
  StmtPtr suite = nullptr;
  if (!in(stmt->attributes, "internal") && !in(stmt->attributes, ".c")) {
    ctx->addBlock();
    if (in(stmt->attributes, "llvm"))
      suite = parseLLVMImport(stmt->suite->firstInBlock());
    else
      suite = TransformVisitor(ctx).transform(stmt->suite);
    ctx->popBlock();
  }

  auto refParent =
      ctx->bases.back().parent == -1 ? "" : ctx->bases[ctx->bases.back().parent].name;
  ctx->bases.pop_back();
  ctx->popBlock();

  string parentFunc = "";
  for (int i = int(ctx->bases.size()) - 1; i >= 0; i--)
    if (!ctx->bases[i].isType()) {
      parentFunc = ctx->bases[i].name;
      break;
    }
  bool isMethod = (ctx->bases.size() && refParent == ctx->bases.back().name);
  if (canonicalName == ".Ptr.__elemsize__" || canonicalName == ".Ptr.__atomic__")
    isMethod = true;

  auto attributes = stmt->attributes;
  // parentFunc: outer function scope (not class)
  // class: outer class scope
  // method: set if function is a method; usually set iff it references
  attributes[".parentFunc"] = parentFunc;
  if (isClassMember) {
    attributes[".class"] = ctx->bases.back().name;
    if (isMethod)
      attributes[".method"] = "";
  }
  resultStmt = N<FunctionStmt>(canonicalName, move(ret), move(newGenerics), move(args),
                               move(suite), move(attributes));
  ctx->cache->asts[canonicalName] = clone(resultStmt);
}

void TransformVisitor::visit(const ClassStmt *stmt) {
  bool extension = in(stmt->attributes, "extend");
  if (extension && stmt->attributes.size() != 1)
    error("extend cannot be mixed with other attributes");
  if (extension && ctx->bases.size())
    error("extend only valid at the toplevel");

  bool isRecord = stmt->isRecord();

  string canonicalName;
  const ClassStmt *originalAST = nullptr;
  if (!extension) {
    canonicalName = ctx->generateCanonicalName(stmt->name);
    if (ctx->bases.size() && ctx->bases.back().isType())
      error("nested classes are not supported");
    if (!isRecord)
      ctx->add(TransformItem::Type, stmt->name, canonicalName, ctx->isToplevel());
    originalAST = stmt;
  } else {
    auto val = ctx->find(stmt->name);
    if (!val && val->kind != TransformItem::Type)
      error("cannot find type {} to extend", stmt->name);
    canonicalName = val->canonicalName;
    const auto &astIter = ctx->cache->asts.find(canonicalName);
    assert(astIter != ctx->cache->asts.end());
    originalAST = CAST(astIter->second, ClassStmt);
    assert(originalAST);
    if (originalAST->generics.size() != stmt->generics.size())
      error("generics do not match");
  }

  ctx->bases.push_back({canonicalName});
  ctx->bases.back().ast = N<IdExpr>(stmt->name);
  if (stmt->generics.size()) {
    vector<ExprPtr> genAst;
    for (int gi = 0; gi < originalAST->generics.size(); gi++)
      genAst.push_back(N<IdExpr>(stmt->generics[gi].name));
    ctx->bases.back().ast =
        N<IndexExpr>(N<IdExpr>(stmt->name), N<TupleExpr>(move(genAst)));
  }

  ctx->addBlock();
  vector<Param> newGenerics;
  for (int gi = 0; gi < originalAST->generics.size(); gi++) {
    if (originalAST->generics[gi].deflt)
      error("default generics not supported in types");
    ctx->add(TransformItem::Type, stmt->generics[gi].name, "", false, true,
             originalAST->generics[gi].type != nullptr);
    newGenerics.push_back(Param{stmt->generics[gi].name,
                                transformType(originalAST->generics[gi].type),
                                transform(originalAST->generics[gi].deflt, true)});
  }
  vector<Param> args;
  auto suite = N<SuiteStmt>(vector<StmtPtr>{});
  if (!extension) {
    unordered_set<string> seenMembers;
    for (auto &a : stmt->args) {
      seqassert(a.type, "no type provided for '{}'", a.name);
      if (seenMembers.find(a.name) != seenMembers.end())
        error(a.type, "'{}' declared twice", a.name);
      seenMembers.insert(a.name);
      args.push_back(Param{a.name, transformType(a.type), nullptr});
    }
    if (isRecord) {
      ctx->popBlock();

      auto old =
          TransformContext::Base{ctx->bases.back().name, clone(ctx->bases.back().ast),
                                 ctx->bases.back().parent};
      ctx->bases.pop_back();
      ctx->add(TransformItem::Type, stmt->name, canonicalName, ctx->isToplevel());
      ctx->bases.push_back({old.name, move(old.ast), old.parent});
      ctx->addBlock();
      for (int gi = 0; gi < originalAST->generics.size(); gi++)
        ctx->add(TransformItem::Type, stmt->generics[gi].name, "", false, true,
                 originalAST->generics[gi].type != nullptr);
    }

    ctx->cache->asts[canonicalName] = N<ClassStmt>(
        canonicalName, move(newGenerics), move(args), N<SuiteStmt>(vector<StmtPtr>()),
        map<string, string>(stmt->attributes));

    vector<StmtPtr> fns;
    ExprPtr codeType = clone(ctx->bases.back().ast);
    vector<string> magics{};
    if (!in(stmt->attributes, "internal")) {
      if (!isRecord) {
        magics = {"new", "init", "raw"};
        if (in(stmt->attributes, "total_ordering"))
          for (auto &i : {"eq", "ne", "lt", "gt", "le", "ge"})
            magics.push_back(i);
        if (!in(stmt->attributes, "no_pickle"))
          for (auto &i : {"pickle", "unpickle"})
            magics.push_back(i);
      } else {
        magics = {"new", "str", "len", "hash"};
        if (!in(stmt->attributes, "no_total_ordering"))
          for (auto &i : {"eq", "ne", "lt", "gt", "le", "ge"})
            magics.push_back(i);
        if (!in(stmt->attributes, "no_pickle"))
          for (auto &i : {"pickle", "unpickle"})
            magics.push_back(i);
        if (!in(stmt->attributes, "no_container"))
          for (auto &i : {"iter", "getitem", "contains"})
            magics.push_back(i);
        if (!in(stmt->attributes, "no_python"))
          for (auto &i : {"to_py", "from_py"})
            magics.push_back(i);
      }
    }
    for (auto &m : magics)
      fns.push_back(codegenMagic(m, ctx->bases.back().ast, stmt->args, isRecord));
    fns.push_back(clone(stmt->suite));
    for (auto &s : fns)
      for (auto &sp : addMethods(s))
        suite->stmts.push_back(move(sp));
  } else {
    for (auto &sp : addMethods(stmt->suite))
      suite->stmts.push_back(move(sp));
  }
  ctx->bases.pop_back();
  ctx->popBlock();

  if (!extension) {
    auto c = static_cast<ClassStmt *>(ctx->cache->asts[canonicalName].get());
    c->suite = move(suite);
    string parentFunc = "";
    for (int i = int(ctx->bases.size()) - 1; i >= 0; i--)
      if (!ctx->bases[i].isType()) {
        parentFunc = ctx->bases[i].name;
        break;
      }
    c->attributes[".parentFunc"] = parentFunc;
    resultStmt = clone(ctx->cache->asts[canonicalName]);
  } else {
    resultStmt = N<ClassStmt>(canonicalName, move(newGenerics), move(args), move(suite),
                              map<string, string>(stmt->attributes));
  }
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
  if (pat->var != "")
    ctx->add(TransformItem::Var, pat->var);
}

void TransformVisitor::visit(const GuardedPattern *pat) {
  resultPattern = N<GuardedPattern>(
      transform(pat->pattern),
      transform(N<CallExpr>(N<DotExpr>(clone(pat->cond), "__bool__"))));
}

void TransformVisitor::visit(const BoundPattern *pat) {
  resultPattern = N<BoundPattern>(pat->var, transform(pat->pattern));
  ctx->add(TransformItem::Var, pat->var);
}

/*************************************************************************************/

ExprPtr TransformVisitor::transformInt(string value, string suffix) {
  auto isdigit = [](const string &s) {
    for (auto c : s)
      if (!std::isdigit(c))
        return false;
    return true;
  };
  auto to_int = [](string s) {
    if (startswith(s, "0b") || startswith(s, "0B"))
      return std::stoull(s.substr(2), nullptr, 2);
    return std::stoull(s, nullptr, 0);
  };
  try {
    if (suffix.empty())
      return N<IntExpr>(to_int(value));
    if (suffix == "u")
      return N<IntExpr>(to_int(value), true);
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
  return transform(
      N<CallExpr>(N<DotExpr>(N<IdExpr>(".int"), format("__suffix_{}__", suffix)),
                  N<StringExpr>(value)));
}

ExprPtr TransformVisitor::parseFString(string value) {
  int braceCount = 0, braceStart = 0;
  vector<ExprPtr> items;
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
        if (code.size() && code.back() == '=') {
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

string TransformVisitor::generateFunctionStub(int len) {
  assert(len >= 1);
  auto typeName = fmt::format("Function.{}", len - 1);
  if (ctx->cache->variardics.find(typeName) == ctx->cache->variardics.end()) {
    ctx->cache->variardics.insert(typeName);

    vector<Param> generics, args;
    vector<ExprPtr> genericNames;
    for (int i = 1; i <= len; i++) {
      genericNames.push_back(N<IdExpr>(format("T{}", i)));
      generics.push_back(Param{format("T{}", i), nullptr, nullptr});
      args.push_back(Param{format(".a{0}", i), N<IdExpr>(format("T{}", i)), nullptr});
    }
    ExprPtr type = N<IdExpr>(typeName);
    if (genericNames.size())
      type = N<IndexExpr>(move(type), N<TupleExpr>(move(genericNames)));

    vector<StmtPtr> fns;
    vector<Param> p;
    p.push_back(Param{"what", N<IndexExpr>(N<IdExpr>("Ptr"), N<IdExpr>("byte"))});
    fns.push_back(make_unique<FunctionStmt>("__new__", clone(type), vector<Param>{},
                                            move(p), nullptr,
                                            vector<string>{"internal"}));
    p.clear();
    p.push_back(Param{"self", clone(type)});
    fns.push_back(make_unique<FunctionStmt>("__str__", N<IdExpr>("str"),
                                            vector<Param>{}, move(p), nullptr,
                                            vector<string>{"internal"}));
    // p.clear();
    // p.push_back({"self", clone(type)});
    // for (int i = 2; i <= len; i++)
    //   p.push_back({format(".a{0}", i), N<IdExpr>(format("T{}", i)), nullptr});
    // fns.push_back(make_unique<FunctionStmt>("__call__", N<IdExpr>("T1"),
    //                                         vector<Param>{}, move(p), nullptr,
    //                                         vector<string>{"internal"}));

    StmtPtr stmt = make_unique<ClassStmt>(typeName, move(generics), clone_nop(args),
                                          N<SuiteStmt>(move(fns)),
                                          vector<string>{"internal", "trait", "tuple"});
    stmt->setSrcInfo(ctx->getGeneratedPos());
    TransformVisitor(make_shared<TransformContext>("<generated>", ctx->cache))
        .transform(stmt);
  }
  return "." + typeName;
}

string TransformVisitor::generateTupleStub(int len) {
  for (int len_i = 0; len_i <= len; len_i++) {
    auto typeName = fmt::format("Tuple.{}", len_i);
    if (ctx->cache->variardics.find(typeName) == ctx->cache->variardics.end()) {
      ctx->cache->variardics.insert(typeName);
      vector<Param> generics, args;
      for (int i = 1; i <= len_i; i++) {
        generics.push_back(Param{format("T{}", i), nullptr, nullptr});
        args.push_back(Param{format("a{0}", i), N<IdExpr>(format("T{}", i)), nullptr});
      }
      StmtPtr stmt = make_unique<ClassStmt>(typeName, move(generics), move(args),
                                            nullptr, vector<string>{"tuple"});
      stmt->setSrcInfo(ctx->getGeneratedPos());
      TransformVisitor(make_shared<TransformContext>("<generated>", ctx->cache))
          .transform(stmt);
    }
  }
  return fmt::format(".Tuple.{}", len);
}

StmtPtr TransformVisitor::getGeneratorBlock(const vector<GeneratorBody> &loops,
                                            SuiteStmt *&prev) {
  StmtPtr suite = N<SuiteStmt>(), newSuite = nullptr;
  prev = (SuiteStmt *)suite.get();
  SuiteStmt *nextPrev = nullptr;
  for (auto &l : loops) {
    newSuite = N<SuiteStmt>();
    nextPrev = (SuiteStmt *)newSuite.get();

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

vector<StmtPtr> TransformVisitor::addMethods(const StmtPtr &s) {
  vector<StmtPtr> v;
  if (!s)
    return v;
  if (auto sp = CAST(s, SuiteStmt)) {
    for (auto &ss : sp->stmts)
      for (auto &u : addMethods(ss))
        v.push_back(move(u));
  } else if (CAST(s, ExprStmt) && CAST(CAST(s, ExprStmt)->expr, StringExpr)) {
  } else if (!CAST(s, FunctionStmt)) {
    error(s, "expected a function (only functions are allowed within type "
             "definitions)");
  } else {
    v.push_back(transform(s));
  }
  return v;
}

StmtPtr TransformVisitor::codegenMagic(const string &op, const ExprPtr &typExpr,
                                       const vector<Param> &args, bool isRecord) {
#define I(s) N<IdExpr>(s)
  ExprPtr ret;
  vector<Param> fargs;
  vector<StmtPtr> stmts;
  vector<string> attrs;
  if (op == "new") {
    ret = clone(typExpr);
    if (isRecord)
      for (auto &a : args)
        fargs.emplace_back(
            Param{a.name, clone(a.type),
                  a.deflt ? clone(a.deflt) : N<CallExpr>(clone(a.type))});
    attrs.emplace_back("internal");
  } else if (op == "init") {
    assert(!isRecord);
    ret = I("void");
    fargs.emplace_back(Param{"self", clone(typExpr)});
    for (auto &a : args) {
      stmts.push_back(N<AssignMemberStmt>(I("self"), a.name, I(a.name)));
      fargs.emplace_back(Param{a.name, clone(a.type),
                               a.deflt ? clone(a.deflt) : N<CallExpr>(clone(a.type))});
    }
  } else if (op == "raw") {
    fargs.emplace_back(Param{"self", clone(typExpr)});
    ret = N<IndexExpr>(N<IdExpr>("Ptr"), N<IdExpr>("byte"));
    attrs.emplace_back("internal");
  } else if (op == "getitem") {
    fargs.emplace_back(Param{"self", clone(typExpr)});
    fargs.emplace_back(Param{"index", I("int")});
    ret = !args.empty() ? clone(args[0].type) : I("void");
    attrs.emplace_back("internal");
  } else if (op == "iter") {
    fargs.emplace_back(Param{"self", clone(typExpr)});
    ret = N<IndexExpr>(I("Generator"), !args.empty() ? clone(args[0].type) : I("void"));
    for (auto &a : args)
      stmts.emplace_back(N<YieldStmt>(N<DotExpr>(N<IdExpr>("self"), a.name)));
  } else if (op == "eq") {
    fargs.emplace_back(Param{"self", clone(typExpr)});
    fargs.emplace_back(Param{"other", clone(typExpr)});
    ret = I("bool");
    for (auto &a : args)
      stmts.push_back(N<IfStmt>(
          N<UnaryExpr>("!",
                       N<CallExpr>(N<DotExpr>(N<DotExpr>(I("self"), a.name), "__eq__"),
                                   N<DotExpr>(I("other"), a.name))),
          N<ReturnStmt>(N<BoolExpr>(false))));
    stmts.emplace_back(N<ReturnStmt>(N<BoolExpr>(true)));
  } else if (op == "ne") {
    fargs.emplace_back(Param{"self", clone(typExpr)});
    fargs.emplace_back(Param{"other", clone(typExpr)});
    ret = I("bool");
    for (auto &a : args)
      stmts.emplace_back(
          N<IfStmt>(N<CallExpr>(N<DotExpr>(N<DotExpr>(I("self"), a.name), "__ne__"),
                                N<DotExpr>(I("other"), a.name)),
                    N<ReturnStmt>(N<BoolExpr>(true))));
    stmts.push_back(N<ReturnStmt>(N<BoolExpr>(false)));
  } else if (op == "lt" || op == "gt") {
    fargs.emplace_back(Param{"self", clone(typExpr)});
    fargs.emplace_back(Param{"other", clone(typExpr)});
    ret = I("bool");
    vector<StmtPtr> *v = &stmts;
    for (int i = 0; i < (int)args.size() - 1; i++) {
      v->emplace_back(N<IfStmt>(
          N<CallExpr>(
              N<DotExpr>(N<DotExpr>(I("self"), args[i].name), format("__{}__", op)),
              N<DotExpr>(I("other"), args[i].name)),
          N<ReturnStmt>(N<BoolExpr>(true)),
          N<CallExpr>(N<DotExpr>(N<DotExpr>(I("self"), args[i].name), "__eq__"),
                      N<DotExpr>(I("other"), args[i].name)),
          N<SuiteStmt>()));
      v = &((SuiteStmt *)(((IfStmt *)(v->back().get()))->ifs.back().suite).get())
               ->stmts;
    }
    if (!args.empty())
      v->emplace_back(N<ReturnStmt>(N<CallExpr>(
          N<DotExpr>(N<DotExpr>(I("self"), args.back().name), format("__{}__", op)),
          N<DotExpr>(I("other"), args.back().name))));
    stmts.emplace_back(N<ReturnStmt>(N<BoolExpr>(false)));
  } else if (op == "le" || op == "ge") {
    fargs.emplace_back(Param{"self", clone(typExpr)});
    fargs.emplace_back(Param{"other", clone(typExpr)});
    ret = I("bool");
    vector<StmtPtr> *v = &stmts;
    for (int i = 0; i < (int)args.size() - 1; i++) {
      v->emplace_back(N<IfStmt>(
          N<UnaryExpr>("!", N<CallExpr>(N<DotExpr>(N<DotExpr>(I("self"), args[i].name),
                                                   format("__{}__", op)),
                                        N<DotExpr>(I("other"), args[i].name))),
          N<ReturnStmt>(N<BoolExpr>(false)),
          N<CallExpr>(N<DotExpr>(N<DotExpr>(I("self"), args[i].name), "__eq__"),
                      N<DotExpr>(I("other"), args[i].name)),
          N<SuiteStmt>()));
      v = &((SuiteStmt *)(((IfStmt *)(v->back().get()))->ifs.back().suite).get())
               ->stmts;
    }
    if (!args.empty())
      v->emplace_back(N<ReturnStmt>(N<CallExpr>(
          N<DotExpr>(N<DotExpr>(I("self"), args.back().name), format("__{}__", op)),
          N<DotExpr>(I("other"), args.back().name))));
    stmts.emplace_back(N<ReturnStmt>(N<BoolExpr>(true)));
  } else if (op == "hash") {
    fargs.emplace_back(Param{"self", clone(typExpr)});
    ret = I("int");
    stmts.emplace_back(N<AssignStmt>(I("seed"), N<IntExpr>(0)));
    for (auto &a : args)
      stmts.push_back(N<UpdateStmt>(
          I("seed"),
          N<BinaryExpr>(
              I("seed"), "^",
              N<BinaryExpr>(
                  N<BinaryExpr>(N<CallExpr>(N<DotExpr>(N<DotExpr>(I("self"), a.name),
                                                       "__hash__")),
                                "+", N<IntExpr>(0x9e3779b9)),
                  "+",
                  N<BinaryExpr>(N<BinaryExpr>(I("seed"), "<<", N<IntExpr>(6)), "+",
                                N<BinaryExpr>(I("seed"), ">>", N<IntExpr>(2)))))));
    stmts.emplace_back(N<ReturnStmt>(I("seed")));
    attrs.emplace_back("delay");
  } else if (op == "pickle") {
    fargs.emplace_back(Param{"self", clone(typExpr)});
    fargs.emplace_back(
        Param{"dest", N<IndexExpr>(N<IdExpr>("Ptr"), N<IdExpr>("byte"))});
    ret = I("void");
    for (auto &a : args)
      stmts.emplace_back(N<ExprStmt>(N<CallExpr>(
          N<DotExpr>(N<DotExpr>(I("self"), a.name), "__pickle__"), I("dest"))));
    attrs.emplace_back("delay");
  } else if (op == "unpickle") {
    fargs.emplace_back(Param{"src", N<IndexExpr>(N<IdExpr>("Ptr"), N<IdExpr>("byte"))});
    ret = clone(typExpr);
    vector<CallExpr::Arg> ar;
    for (auto &a : args)
      ar.emplace_back(CallExpr::Arg{
          "", N<CallExpr>(N<DotExpr>(clone(a.type), "__unpickle__"), I("src"))});
    stmts.emplace_back(N<ReturnStmt>(N<CallExpr>(clone(typExpr), move(ar))));
    attrs.emplace_back("delay");
  } else if (op == "len") {
    fargs.emplace_back(Param{"self", clone(typExpr)});
    ret = I("int");
    stmts.emplace_back(N<ReturnStmt>(N<IntExpr>(args.size())));
  } else if (op == "contains") {
    fargs.emplace_back(Param{"self", clone(typExpr)});
    fargs.emplace_back(Param{"what", args.size() ? clone(args[0].type) : I("void")});
    ret = I("bool");
    attrs.emplace_back("internal");
  } else if (op == "to_py") {
    fargs.emplace_back(Param{"self", clone(typExpr)});
    ret = I("pyobj");
    stmts.emplace_back(
        N<AssignStmt>(I("o"), N<CallExpr>(N<DotExpr>(I("pyobj"), "_tuple_new"),
                                          N<IntExpr>(args.size()))));
    for (int i = 0; i < args.size(); i++)
      stmts.push_back(N<ExprStmt>(N<CallExpr>(
          N<DotExpr>(I("o"), "_tuple_set"), N<IntExpr>(i),
          N<CallExpr>(N<DotExpr>(N<DotExpr>(I("self"), args[i].name), "__to_py__")))));
    stmts.emplace_back(N<ReturnStmt>(I("o")));
    attrs.emplace_back("delay");
  } else if (op == "from_py") {
    fargs.emplace_back(Param{"src", I("pyobj")});
    ret = clone(typExpr);
    vector<CallExpr::Arg> ar;
    for (int i = 0; i < args.size(); i++)
      ar.push_back(CallExpr::Arg{
          "",
          N<CallExpr>(N<DotExpr>(clone(args[i].type), "__from_py__"),
                      N<CallExpr>(N<DotExpr>(I("src"), "_tuple_get"), N<IntExpr>(i)))});
    stmts.emplace_back(N<ReturnStmt>(N<CallExpr>(clone(typExpr), move(ar))));
    attrs.emplace_back("delay");
  } else if (op == "str") {
    fargs.emplace_back(Param{"self", clone(typExpr)});
    ret = I("str");
    if (!args.empty()) {
      stmts.emplace_back(
          N<AssignStmt>(I("a"), N<CallExpr>(N<IndexExpr>(I("__array__"), I("str")),
                                            N<IntExpr>(args.size()))));
      for (int i = 0; i < args.size(); i++)
        stmts.push_back(N<ExprStmt>(N<CallExpr>(
            N<DotExpr>(I("a"), "__setitem__"), N<IntExpr>(i),
            N<CallExpr>(N<DotExpr>(N<DotExpr>(I("self"), args[i].name), "__str__")))));
      stmts.emplace_back(N<ReturnStmt>(N<CallExpr>(N<DotExpr>(I("str"), "_tuple_str"),
                                                   N<DotExpr>(I("a"), "ptr"),
                                                   N<IntExpr>(args.size()))));
    } else {
      stmts.emplace_back(N<ReturnStmt>(N<StringExpr>("()")));
    }
  } else {
    seqassert(false, "invalid magic {}", op);
  }
#undef I
  auto t =
      make_unique<FunctionStmt>(format("__{}__", op), move(ret), vector<Param>{},
                                move(fargs), N<SuiteStmt>(move(stmts)), move(attrs));
  t->setSrcInfo(ctx->getGeneratedPos());
  return t;
}

ExprPtr TransformVisitor::makeAnonFn(vector<StmtPtr> &&stmts,
                                     const vector<string> &vars) {
  vector<Param> params;
  vector<CallExpr::Arg> args;

  string name = getTemporaryVar("lambda", '.');
  ctx->captures.push_back({});
  for (auto &s : vars)
    params.emplace_back(Param{s, nullptr, nullptr});
  auto fs =
      transform(N<FunctionStmt>(name, nullptr, vector<Param>{}, move(params),
                                N<SuiteStmt>(move(stmts)), vector<string>{".inline"}));
  auto f = CAST(fs, FunctionStmt);
  for (auto &c : ctx->captures.back()) {
    f->args.emplace_back(Param{c, nullptr, nullptr});
    args.push_back({"", N<IdExpr>(c)});
  }
  ((FunctionStmt *)(ctx->cache->asts[f->name].get()))->args = clone_nop(f->args);
  ctx->captures.pop_back();

  prependStmts->push_back(move(fs));

  return N<CallExpr>(N<IdExpr>(name), move(args));
}

StmtPtr TransformVisitor::parseCImport(string name, const vector<Param> &args,
                                       const ExprPtr &ret, string altName,
                                       StringExpr *code) {
  auto canonicalName = ctx->generateCanonicalName(name);
  vector<Param> fnArgs;
  vector<TypePtr> argTypes{};
  generateFunctionStub(args.size() + 1);
  for (int ai = 0; ai < args.size(); ai++) {
    if (args[ai].deflt)
      error("default arguments not supported here");
    if (!args[ai].type)
      error("type for '{}' not specified", args[ai].name);
    fnArgs.emplace_back(
        Param{args[ai].name.empty() ? format(".a{}", ai) : args[ai].name,
              transformType(args[ai].type), nullptr});
  }
  ctx->add(TransformItem::Func, altName.empty() ? name : altName, canonicalName,
           ctx->isToplevel());
  StmtPtr body = code ? N<ExprStmt>(code->clone()) : nullptr;
  if (code && !ret)
    error("LLVM functions must have a return type");
  auto f = N<FunctionStmt>(
      canonicalName, ret ? transformType(ret) : transformType(N<IdExpr>("void")),
      vector<Param>(), move(fnArgs), move(body), vector<string>{code ? "llvm" : ".c"});
  ctx->cache->asts[canonicalName] = clone(f);
  return f;
}

StmtPtr TransformVisitor::parseDylibCImport(const ExprPtr &dylib, string name,
                                            const vector<Param> &args,
                                            const ExprPtr &ret, string altName) {
  vector<StmtPtr> stmts;
  stmts.push_back(
      N<AssignStmt>(N<IdExpr>("fptr"), N<CallExpr>(N<IdExpr>("_dlsym"), clone(dylib),
                                                   N<StringExpr>(name))));
  vector<ExprPtr> fnArgs;
  fnArgs.push_back(ret ? clone(ret) : N<IdExpr>("void"));
  for (auto &a : args)
    fnArgs.push_back(clone(a.type));
  stmts.push_back(N<AssignStmt>(
      N<IdExpr>("f"),
      N<CallExpr>(N<IndexExpr>(N<IdExpr>("Function"), N<TupleExpr>(move(fnArgs))),
                  N<IdExpr>("fptr"))));
  bool isVoid = true;
  if (ret) {
    if (auto f = CAST(ret, IdExpr))
      isVoid = f->value == "void";
    else
      isVoid = false;
  }
  fnArgs.clear();
  for (int i = 0; i < args.size(); i++)
    fnArgs.push_back(N<IdExpr>(args[i].name != "" ? args[i].name : format(".a{}", i)));
  auto call = N<CallExpr>(N<IdExpr>("f"), move(fnArgs));
  if (!isVoid)
    stmts.push_back(N<ReturnStmt>(move(call)));
  else
    stmts.push_back(N<ExprStmt>(move(call)));
  vector<Param> params;
  for (int i = 0; i < args.size(); i++)
    params.emplace_back(Param{args[i].name != "" ? args[i].name : format(".a{}", i),
                              clone(args[i].type)});
  return transform(N<FunctionStmt>(altName.empty() ? name : altName, clone(ret),
                                   vector<Param>(), move(params),
                                   N<SuiteStmt>(move(stmts)), vector<string>()));
}

// from python import X.Y -> import X; from X import Y ... ?
// from python import Y -> get Y? works---good! not: import Y; return import
StmtPtr TransformVisitor::parsePythonImport(const ExprPtr &what, string as) {
  vector<StmtPtr> stmts;
  string from = "";

  vector<string> dirs;
  Expr *e = what.get();
  while (auto d = dynamic_cast<DotExpr *>(e)) {
    dirs.push_back(d->member);
    e = d->expr.get();
  }
  if (!e->getId())
    error("invalid import statement");
  dirs.push_back(e->getId()->value);
  string name = dirs[0], lib;
  for (int i = dirs.size() - 1; i > 0; i--)
    lib += dirs[i] + (i > 1 ? "." : "");
  return transform(N<AssignStmt>(
      N<IdExpr>(name), N<CallExpr>(N<DotExpr>(N<IdExpr>("pyobj"), "_py_import"),
                                   N<StringExpr>(name), N<StringExpr>(lib))));
  // imp = pyobj._py_import("foo", "lib")
}

StmtPtr TransformVisitor::parseLLVMImport(const Stmt *codeStmt) {
  if (!codeStmt->getExpr() || !codeStmt->getExpr()->expr->getString())
    error("invalid LLVM function");

  auto code = codeStmt->getExpr()->expr->getString()->value;
  vector<StmtPtr> items;
  auto se = N<StringExpr>("");
  string &finalCode = se->value;
  items.push_back(N<ExprStmt>(move(se)));

  auto escape = [](const string &str, int s, int l) {
    string t;
    t.reserve(l);
    for (int i = s; i < s + l; i++)
      if (str[i] == '{')
        t += "{{";
      else if (str[i] == '}')
        t += "}}";
      else
        t += str[i];
    return t;
  };

  int braceCount = 0, braceStart = 0;
  for (int i = 0; i < code.size(); i++) {
    if (i < code.size() - 1 && code[i] == '{' && code[i + 1] == '=') {
      if (braceStart < i)
        finalCode += escape(code, braceStart, i - braceStart) + '{';
      if (!braceCount) {
        braceStart = i + 2;
        braceCount++;
      } else {
        error("invalid LLVM substitution");
      }
    } else if (braceCount && code[i] == '}') {
      braceCount--;
      string exprCode = code.substr(braceStart, i - braceStart);
      auto offset = getSrcInfo();
      offset.col += i;
      auto expr = transformGenericExpr(parseExpr(exprCode, offset));
      if (!expr->isType() && !expr->getStatic())
        error(expr, "expression {} is not a type or static expression",
              expr->toString());
      //        LOG("~~> {} -> {}", exprCode, expr->toString());
      items.push_back(N<ExprStmt>(move(expr)));
      braceStart = i + 1;
      finalCode += '}';
    }
  }
  if (braceCount)
    error("invalid LLVM substitution");
  if (braceStart != code.size())
    finalCode += escape(code, braceStart, code.size() - braceStart);

  return N<SuiteStmt>(move(items));
}

ExprPtr TransformVisitor::transformGenericExpr(const ExprPtr &i) {
  auto t = transform(i, true);
  set<string> captures;
  if (isStaticExpr(t, captures))
    return N<StaticExpr>(clone(t), move(captures));
  else
    return t;
}

bool TransformVisitor::isStaticExpr(const ExprPtr &expr, set<string> &captures) {
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
           isStaticExpr(eb->lexpr, captures) && isStaticExpr(eb->rexpr, captures);
  } else if (auto eu = CAST(expr, UnaryExpr)) {
    return ((eu->op == "-") || (eu->op == "!")) && isStaticExpr(eu->expr, captures);
  } else if (auto ef = CAST(expr, IfExpr)) {
    return isStaticExpr(ef->cond, captures) && isStaticExpr(ef->ifexpr, captures) &&
           isStaticExpr(ef->elsexpr, captures);
  } else if (auto eit = expr->getInt()) {
    if (eit->suffix.size())
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

} // namespace ast
} // namespace seq
