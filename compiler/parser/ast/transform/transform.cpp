/**
 * TODO : Redo error messages (right now they are awful)
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
using std::make_shared;
using std::make_unique;
using std::move;
using std::ostream;
using std::pair;
using std::set;
using std::shared_ptr;
using std::stack;
using std::static_pointer_cast;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::unordered_set;
using std::vector;

namespace seq {
namespace ast {

using namespace types;

TransformVisitor::TransformVisitor(shared_ptr<TransformContext> ctx,
                                   shared_ptr<vector<StmtPtr>> stmts)
    : ctx(ctx) {
  prependStmts = stmts ? stmts : make_shared<vector<StmtPtr>>();
}

StmtPtr TransformVisitor::apply(shared_ptr<Cache> cache, StmtPtr s) {
  auto suite = make_unique<SuiteStmt>();
  suite->stmts.push_back(make_unique<SuiteStmt>());
  auto *preamble = (SuiteStmt *)(suite->stmts[0].get());

  if (cache->imports.find("") == cache->imports.end()) {
    auto stdlib = make_shared<TransformContext>("", cache);
    auto stdlibPath = stdlib->findFile("core", "", true);
    if (stdlibPath == "")
      ast::error("cannot load standard library");
    stdlib->setFilename(stdlibPath);
    cache->imports[""] = {stdlibPath, stdlib};

    // Add preamble for core types and variardic stubs
    stdlib->add(TransformItem::Type, "void", "void", true);
    for (auto &name : {"bool", "byte", "int", "float"}) {
      auto canonical = stdlib->generateCanonicalName(name);
      stdlib->add(TransformItem::Type, name, canonical, true);
      cache->asts[canonical] =
          make_unique<ClassStmt>(true, canonical, vector<Param>(), vector<Param>(),
                                 nullptr, vector<string>{"internal"});
      preamble->stmts.push_back(clone(cache->asts[canonical]));
    }
    for (auto &name : {"Ptr", "Generator", "Optional", "Int", "UInt"}) {
      auto canonical = stdlib->generateCanonicalName(name);
      stdlib->add(TransformItem::Type, name, canonical, true);
      vector<Param> generics;
      generics.push_back({"T",
                          string(name) == "Int" || string(name) == "UInt"
                              ? make_unique<IdExpr>("int")
                              : nullptr,
                          nullptr});
      cache->asts[canonical] =
          make_unique<ClassStmt>(true, canonical, move(generics), vector<Param>(),
                                 nullptr, vector<string>{"internal"});
      preamble->stmts.push_back(clone(cache->asts[canonical]));
    }

    StmtPtr stmts = nullptr;
    stdlib->setFlag("internal");
    assert(stdlibPath.substr(stdlibPath.size() - 12) == "__init__.seq");
    auto internal = stdlibPath.substr(0, stdlibPath.size() - 12) + "__internal__.seq";
    stdlib->setFilename(internal);
    // Load core aliases
    auto code = "cobj = Ptr[byte]\n@internal\ntype pyobj(p: cobj)\n"
                "@internal\ntype str(len: int, ptr: Ptr[byte])\n";
    stmts = parseCode(internal, code);
    preamble->stmts.push_back(TransformVisitor(stdlib).transform(stmts));
    // Load __internal__
    stmts = parseFile(internal);
    suite->stmts.push_back(TransformVisitor(stdlib).transform(stmts));
    stdlib->unsetFlag("internal");
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
  auto stmts = TransformVisitor(ctx).transform(s);

  preamble->stmts.push_back(clone(cache->asts[".Function.1"])); // main dependency
  for (auto &s : cache->variardics)
    if (s != ".Function.1")
      preamble->stmts.push_back(clone(cache->asts["." + s]));
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
  if (v.prependStmts->size()) {
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

void TransformVisitor::prepend(StmtPtr s) {
  if (auto t = transform(s))
    prependStmts->push_back(move(t));
}

void TransformVisitor::defaultVisit(const Expr *e) { resultExpr = e->clone(); }

void TransformVisitor::defaultVisit(const Stmt *s) { resultStmt = s->clone(); }

void TransformVisitor::defaultVisit(const Pattern *p) { resultPattern = p->clone(); }

/*************************************************************************************/

void TransformVisitor::visit(const NoneExpr *expr) {
  resultExpr = transform(N<CallExpr>(N<IdExpr>(".Optional")));
}

void TransformVisitor::visit(const IntExpr *expr) {
  resultExpr = expr->clone();
  auto i = (IntExpr *)resultExpr.get();
  try {
    if (expr->suffix == "u") {
      i->intValue = std::stoull(expr->value, nullptr, 0);
      i->sign = true;
    } else if (expr->suffix == "") {
      i->intValue = std::stoull(expr->value, nullptr, 0);
    } else {
      string fnName = format("__int_suffix_{}__", expr->suffix);
      if (ctx->find(fnName))
        resultExpr =
            transform(N<CallExpr>(N<IdExpr>(fnName), N<StringExpr>(expr->value)));
      else
        error("unknown suffix '{}'", expr->suffix);
    }
  } catch (std::out_of_range &) {
    error("integer {} out of range", expr->value);
  }
}

void TransformVisitor::visit(const FStringExpr *expr) {
  int braceCount = 0, braceStart = 0;
  vector<ExprPtr> items;
  for (int i = 0; i < expr->value.size(); i++) {
    if (expr->value[i] == '{') {
      if (braceStart < i)
        items.push_back(N<StringExpr>(expr->value.substr(braceStart, i - braceStart)));
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
        items.push_back(N<CallExpr>(N<IdExpr>("str"), parseExpr(code, offset)));
      }
      braceStart = i + 1;
    }
  }
  if (braceCount)
    error("f-string braces are not balanced");
  if (braceStart != expr->value.size())
    items.push_back(
        N<StringExpr>(expr->value.substr(braceStart, expr->value.size() - braceStart)));
  resultExpr = transform(
      N<CallExpr>(N<DotExpr>(N<IdExpr>("str"), "cat"), N<ListExpr>(move(items))));
}

void TransformVisitor::visit(const KmerExpr *expr) {
  resultExpr = transform(
      N<CallExpr>(N<IndexExpr>(N<IdExpr>(".Kmer"), N<IntExpr>(expr->value.size())),
                  N<SeqExpr>(expr->value)));
}

void TransformVisitor::visit(const SeqExpr *expr) {
  if (expr->prefix == "p")
    resultExpr = transform(N<CallExpr>(N<IdExpr>(".pseq"), N<StringExpr>(expr->value)));
  else if (expr->prefix == "s")
    resultExpr = transform(N<CallExpr>(N<IdExpr>(".seq"), N<StringExpr>(expr->value)));
  else
    error("invalid prefix '{}'", expr->prefix);
}

void TransformVisitor::visit(const IdExpr *expr) {
  auto val = ctx->find(expr->value);
  if (!val)
    error("identifier '{}' not found", expr->value);
  if (val->isVar()) {
    if (ctx->getBase() != val->getBase() && !val->isGlobal()) {
      if (ctx->captures.size())
        ctx->captures.back().insert(expr->value);
      else
        error("cannot access non-toplevel variable '{}'", expr->value);
    }
  }

  resultExpr = N<IdExpr>(val->canonicalName.empty() ? expr->value : val->canonicalName);
  if (val->isType() && !val->isStatic())
    resultExpr->markType();

  // Check if function references the outer class generic
  if (val->isGeneric() && ctx->bases.size() > 1) {
    const auto &grandparent = ctx->bases[ctx->bases.size() - 2];
    if (grandparent.isType() && grandparent.name == val->getBase())
      ctx->bases.back().referencesParent = true;
  }
}

void TransformVisitor::visit(const UnpackExpr *expr) {
  resultExpr = transform(N<CallExpr>(N<IdExpr>(".list"), clone(expr->what)));
}

void TransformVisitor::visit(const TupleExpr *expr) {
  auto name = generateTupleStub(expr->items.size());
  resultExpr = transform(
      N<CallExpr>(N<DotExpr>(N<IdExpr>(name), "__new__"), transform(expr->items)));
}

void TransformVisitor::visit(const ListExpr *expr) {
  string listVar = getTemporaryVar("lst");
  prepend(N<AssignStmt>(
      N<IdExpr>(listVar),
      N<CallExpr>(N<IdExpr>(".list"),
                  expr->items.size() ? N<IntExpr>(expr->items.size()) : nullptr)));
  for (int i = 0; i < expr->items.size(); i++)
    prepend(N<ExprStmt>(
        N<CallExpr>(N<DotExpr>(N<IdExpr>(listVar), "append"), clone(expr->items[i]))));
  resultExpr = transform(N<IdExpr>(listVar));
}

void TransformVisitor::visit(const SetExpr *expr) {
  string setVar = getTemporaryVar("set");
  prepend(N<AssignStmt>(N<IdExpr>(setVar), N<CallExpr>(N<IdExpr>(".set"))));
  for (int i = 0; i < expr->items.size(); i++)
    prepend(N<ExprStmt>(
        N<CallExpr>(N<DotExpr>(N<IdExpr>(setVar), "add"), clone(expr->items[i]))));
  resultExpr = transform(N<IdExpr>(setVar));
}

void TransformVisitor::visit(const DictExpr *expr) {
  string dictVar = getTemporaryVar("dict");
  prepend(N<AssignStmt>(N<IdExpr>(dictVar), N<CallExpr>(N<IdExpr>(".dict"))));
  for (int i = 0; i < expr->items.size(); i++)
    prepend(N<ExprStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(dictVar), "__setitem__"),
                                    clone(expr->items[i].key),
                                    clone(expr->items[i].value))));
  resultExpr = transform(N<IdExpr>(dictVar));
}

void TransformVisitor::visit(const GeneratorExpr *expr) {
  SuiteStmt *prev;
  auto suite = getGeneratorBlock(expr->loops, prev);
  string var = getTemporaryVar("gen");
  if (expr->kind == GeneratorExpr::ListGenerator) {
    prepend(N<AssignStmt>(N<IdExpr>(var), N<CallExpr>(N<IdExpr>(".list"))));
    prev->stmts.push_back(N<ExprStmt>(
        N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "append"), clone(expr->expr))));
    prepend(move(suite));
  } else if (expr->kind == GeneratorExpr::SetGenerator) {
    prepend(N<AssignStmt>(N<IdExpr>(var), N<CallExpr>(N<IdExpr>(".set"))));
    prev->stmts.push_back(
        N<ExprStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>(var), "add"), clone(expr->expr))));
    prepend(move(suite));
  } else {
    prev->stmts.push_back(N<YieldStmt>(clone(expr->expr)));
    string fnVar = getTemporaryVar("anonGen");

    ctx->captures.push_back({});
    prepend(N<FunctionStmt>(fnVar, nullptr, vector<Param>{}, vector<Param>(),
                            move(suite), vector<string>{}));
    assert(prependStmts->size());
    auto f = CAST(prependStmts->back(), FunctionStmt);
    assert(f);
    vector<Param> params;
    vector<CallExpr::Arg> args;
    for (auto &c : ctx->captures.back()) {
      params.push_back({c, nullptr, nullptr});
      args.push_back({"", N<IdExpr>(c)});
    }
    ctx->captures.pop_back();
    f->args = clone_nop(params);
    static_cast<FunctionStmt *>(ctx->cache->asts[f->name].get())->args =
        clone_nop(params);

    prepend(N<AssignStmt>(N<IdExpr>(var),
                          N<CallExpr>(N<DotExpr>(
                              N<CallExpr>(N<IdExpr>(fnVar), move(args)), "__iter__"))));
  }
  resultExpr = transform(N<IdExpr>(var));
}

void TransformVisitor::visit(const DictGeneratorExpr *expr) {
  SuiteStmt *prev;
  auto suite = getGeneratorBlock(expr->loops, prev);
  string var = getTemporaryVar("gen");
  prepend(N<AssignStmt>(N<IdExpr>(var), N<CallExpr>(N<IdExpr>(".dict"))));
  prev->stmts.push_back(N<ExprStmt>(N<CallExpr>(
      N<DotExpr>(N<IdExpr>(var), "__setitem__"), clone(expr->key), clone(expr->expr))));
  prepend(move(suite));
  resultExpr = transform(N<IdExpr>(var));
}

void TransformVisitor::visit(const IfExpr *expr) {
  resultExpr =
      N<IfExpr>(transform(N<CallExpr>(N<DotExpr>(clone(expr->cond), "__bool__"))),
                transform(expr->eif), transform(expr->eelse));
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
  if (expr->op == "&&") {
    resultExpr = transform(N<CallExpr>(
        N<DotExpr>(N<CallExpr>(N<DotExpr>(clone(expr->lexpr), "__bool__")), "__and__"),
        N<CallExpr>(N<DotExpr>(clone(expr->rexpr), "__bool__"))));
  } else if (expr->op == "||") {
    resultExpr = transform(N<CallExpr>(
        N<DotExpr>(N<CallExpr>(N<DotExpr>(clone(expr->lexpr), "__bool__")), "__or__"),
        N<CallExpr>(N<DotExpr>(clone(expr->rexpr), "__bool__"))));
  } else if (expr->op == "is not") {
    resultExpr = transform(N<CallExpr>(
        N<DotExpr>(N<BinaryExpr>(transform(expr->lexpr), "is", transform(expr->rexpr)),
                   "__invert__")));
  } else if (expr->op == "not in") {
    resultExpr = transform(
        N<UnaryExpr>("!", N<CallExpr>(N<DotExpr>(clone(expr->rexpr), "__contains__"),
                                      clone(expr->lexpr))));
  } else if (expr->op == "in") {
    resultExpr = transform(N<CallExpr>(N<DotExpr>(clone(expr->rexpr), "__contains__"),
                                       clone(expr->lexpr)));
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
  if (auto i = CAST(expr->expr, IdExpr)) { // special case: tuples and functions
    if (i->value == "tuple" || i->value == "Tuple") {
      auto t = CAST(expr->index, TupleExpr);
      int items = t ? t->items.size() : 1;
      auto name = generateTupleStub(items);
      e = transformType(N<IdExpr>(name));
      e->markType();
    } else if (i->value == "function" || i->value == "Function") {
      auto t = CAST(expr->index, TupleExpr);
      int items = t ? t->items.size() : 1;
      auto name = generateFunctionStub(items);
      e = transformType(N<IdExpr>(name));
      e->markType();
    }
  }
  if (!e)
    e = transform(expr->expr, true);
  bool isFunc = false;
  if (auto i = CAST(e, IdExpr)) {
    auto val = ctx->find(i->value);
    if (val && val->isFunc())
      isFunc = true;
  }
  if (e->isType() || isFunc) {
    unordered_set<string> supported{"<",  "<=", ">", ">=", "==", "!=", "&&",
                                    "||", "+",  "-", "*",  "//", "%"};
    function<bool(const ExprPtr &, set<string> &)> isStatic =
        [&](const ExprPtr &e, set<string> &captures) -> bool {
      if (auto i = CAST(e, IdExpr)) {
        auto val = ctx->find(i->value);
        if (val && val->isStatic()) {
          captures.insert(i->value);
          return true;
        }
        return false;
      } else if (auto i = CAST(e, BinaryExpr)) {
        return (supported.find(i->op) != supported.end()) &&
               isStatic(i->lexpr, captures) && isStatic(i->rexpr, captures);
      } else if (auto i = CAST(e, UnaryExpr)) {
        return ((i->op == "-") || (i->op == "!")) && isStatic(i->expr, captures);
      } else if (auto i = CAST(e, IfExpr)) {
        return isStatic(i->cond, captures) && isStatic(i->eif, captures) &&
               isStatic(i->eelse, captures);
      } else if (auto i = CAST(e, IntExpr)) {
        if (i->suffix.size())
          return false;
        try {
          std::stoull(i->value, nullptr, 0);
        } catch (std::out_of_range &) {
          return false;
        }
        return true;
      } else {
        return false;
      }
    };
    auto transformGeneric = [&](const ExprPtr &i) -> ExprPtr {
      auto t = transform(i, true);
      set<string> captures;
      if (isStatic(i, captures))
        return N<StaticExpr>(clone(i), captures);
      else if (t->isType())
        return t;
      error("must be a type or a static expression");
      return nullptr;
    };
    vector<ExprPtr> it;
    if (auto t = CAST(expr->index, TupleExpr))
      for (auto &i : t->items)
        it.push_back(transformGeneric(i));
    else
      it.push_back(transformGeneric(expr->index));
    resultExpr = N<InstantiateExpr>(move(e), move(it));
    resultExpr->markType();
  } else {
    resultExpr = N<IndexExpr>(move(e), transform(expr->index));
  }
}

void TransformVisitor::visit(const CallExpr *expr) {
  if (auto ix = CAST(expr->expr, IndexExpr))
    if (auto id = CAST(ix->expr, IdExpr))
      if (id->value == "__array__") {
        if (expr->args.size() != 1)
          error("__array__ requires only size argument");
        resultExpr =
            N<StackAllocExpr>(transformType(ix->index), transform(expr->args[0].value));
        return;
      }
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
  resultExpr =
      N<SliceExpr>(transform(expr->st), transform(expr->ed), transform(expr->step));
}

void TransformVisitor::visit(const TypeOfExpr *expr) {
  resultExpr = N<TypeOfExpr>(transform(expr->expr, true));
  resultExpr->markType();
}

void TransformVisitor::visit(const PtrExpr *expr) {
  if (auto i = CAST(expr->expr, IdExpr)) {
    auto v = ctx->find(i->value);
    if (v && v->isVar()) {
      resultExpr = N<PtrExpr>(transform(expr->expr));
      return;
    }
  }
  error("__ptr__ requires a variable");
}

void TransformVisitor::visit(const LambdaExpr *expr) {
  vector<Param> params;
  unordered_set<string> used;
  for (auto &s : expr->vars)
    params.push_back({s, nullptr, nullptr});

  string fnVar = getTemporaryVar("anonFn");
  ctx->captures.push_back({});
  prepend(N<FunctionStmt>(fnVar, nullptr, vector<Param>{}, move(params),
                          N<ReturnStmt>(clone(expr->expr)), vector<string>{}));
  assert(prependStmts->size());
  auto f = CAST(prependStmts->back(), FunctionStmt);
  assert(f);
  params.clear();
  vector<CallExpr::Arg> args;
  for (auto &s : expr->vars)
    params.push_back({s, nullptr, nullptr});
  for (auto &c : ctx->captures.back()) {
    params.push_back({c, nullptr, nullptr});
    args.push_back({"", N<IdExpr>(c)});
  }
  ctx->captures.pop_back();
  f->args = clone_nop(params);
  static_cast<FunctionStmt *>(ctx->cache->asts[f->name].get())->args =
      clone_nop(params);

  if (args.size()) { // create partial call
    for (int i = 0; i < expr->vars.size(); i++)
      args.insert(args.begin(), {"", N<EllipsisExpr>()});
    resultExpr = transform(N<CallExpr>(N<IdExpr>(fnVar), move(args)));
  } else {
    resultExpr = transform(N<IdExpr>(fnVar));
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

void TransformVisitor::visit(const ExprStmt *stmt) {
  resultStmt = N<ExprStmt>(transform(stmt->expr));
}

void TransformVisitor::visit(const AssignStmt *stmt) {
  auto add = [&](const ExprPtr &lhs, const ExprPtr &rhs, const ExprPtr &type,
                 bool force) -> StmtPtr {
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
      auto s = Nx<AssignStmt>(p, clone(lhs), transform(rhs, true), transformType(type),
                              false, force);
      if (!force && !s->type) {
        auto val = ctx->find(l->value);
        if (val && val->isVar()) {
          if (val->getBase() == ctx->getBase())
            return Nx<UpdateStmt>(p, transform(lhs), move(s->rhs));
          else if (stmt->mustExist)
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
  function<void(const ExprPtr &, const ExprPtr &, vector<StmtPtr> &, bool)> process =
      [&](const ExprPtr &lhs, const ExprPtr &rhs, vector<StmtPtr> &stmts,
          bool force) -> void {
    vector<ExprPtr> lefts;
    if (auto l = CAST(lhs, TupleExpr)) {
      for (auto &i : l->items)
        lefts.push_back(clone(i));
    } else if (auto l = CAST(lhs, ListExpr)) {
      for (auto &i : l->items)
        lefts.push_back(clone(i));
    } else {
      stmts.push_back(add(lhs, rhs, nullptr, force));
      return;
    }
    auto p = rhs.get();
    ExprPtr newRhs = nullptr;
    if (!CAST(rhs, IdExpr)) { // store any non-trivial expression
      auto var = getTemporaryVar("assign");
      newRhs = Nx<IdExpr>(p, var);
      stmts.push_back(add(newRhs, rhs, nullptr, force));
    } else {
      newRhs = clone(rhs);
    }
    UnpackExpr *unpack = nullptr;
    int st = 0;
    for (; st < lefts.size(); st++) {
      if (auto u = CAST(lefts[st], UnpackExpr)) {
        unpack = u;
        break;
      }
      process(move(lefts[st]), Nx<IndexExpr>(p, clone(newRhs), Nx<IntExpr>(p, st)),
              stmts, force);
    }
    if (unpack) {
      process(move(unpack->what),
              Nx<IndexExpr>(p, clone(newRhs),
                            Nx<SliceExpr>(p, Nx<IntExpr>(p, st),
                                          lefts.size() == st + 1
                                              ? nullptr
                                              : Nx<IntExpr>(p, -lefts.size() + st + 1),
                                          nullptr)),
              stmts, force);
      st += 1;
      for (; st < lefts.size(); st++) {
        if (CAST(lefts[st], UnpackExpr))
          error(lefts[st], "multiple unpack expressions found");
        process(move(lefts[st]),
                Nx<IndexExpr>(p, clone(newRhs), Nx<IntExpr>(p, -lefts.size() + st)),
                stmts, force);
      }
    }
  };

  vector<StmtPtr> stmts;
  if (stmt->type) {
    if (auto i = CAST(stmt->lhs, IdExpr))
      stmts.push_back(add(stmt->lhs, stmt->rhs, stmt->type, stmt->force));
    else
      error("invalid type specifier");
  } else {
    process(stmt->lhs, stmt->rhs, stmts, stmt->force);
  }
  resultStmt = stmts.size() == 1 ? move(stmts[0]) : N<SuiteStmt>(move(stmts));
}

void TransformVisitor::visit(const AssignEqStmt *stmt) {
  resultStmt = transform(
      N<AssignStmt>(clone(stmt->lhs),
                    N<BinaryExpr>(clone(stmt->lhs), stmt->op, clone(stmt->rhs), true),
                    nullptr, true));
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
  resultStmt = N<WhileStmt>(transform(cond), transform(stmt->suite));
}

void TransformVisitor::visit(const ForStmt *stmt) {
  ExprPtr iter = N<CallExpr>(N<DotExpr>(clone(stmt->iter), "__iter__"));
  ctx->addBlock();
  if (auto i = CAST(stmt->var, IdExpr)) {
    string varName = i->value;
    ctx->add(TransformItem::Var, varName);
    resultStmt =
        N<ForStmt>(transform(stmt->var), transform(iter), transform(stmt->suite));
  } else {
    string varName = getTemporaryVar("for");
    ctx->add(TransformItem::Var, varName);
    auto var = N<IdExpr>(varName);
    vector<StmtPtr> stmts;
    stmts.push_back(N<AssignStmt>(clone(stmt->var), clone(var), nullptr, false,
                                  /* force */ true));
    stmts.push_back(clone(stmt->suite));
    resultStmt =
        N<ForStmt>(clone(var), transform(iter), transform(N<SuiteStmt>(move(stmts))));
  }
  ctx->popBlock();
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
  auto file = ctx->findFile(stmt->from.first, ctx->getFilename());
  if (file.empty())
    error("cannot locate import '{}'", stmt->from.first);

  auto import = ctx->cache->imports.find(file);
  if (import == ctx->cache->imports.end()) {
    auto ictx = make_shared<TransformContext>(file, ctx->cache);
    import = ctx->cache->imports.insert({file, {file, ictx}}).first;
    StmtPtr s = parseFile(file);
    auto sn = TransformVisitor(ictx).transform(s);
    resultStmt = N<SuiteStmt>(move(sn), true);
  }

  if (!stmt->what.size()) {
    ctx->add(TransformItem::Import,
             stmt->from.second == "" ? stmt->from.first : stmt->from.second, file);
  } else if (stmt->what.size() == 1 && stmt->what[0].first == "*") {
    if (stmt->what[0].second != "")
      error("cannot rename star-import");
    for (auto &i : *(import->second.ctx))
      if (i.second.front()->isGlobal())
        ctx->add(i.first, i.second.front());
  } else {
    for (auto &w : stmt->what) {
      auto c = import->second.ctx->find(w.first);
      if (!c || !c->isGlobal())
        error("symbol '{}' not found in {}", w.first, file);
      ctx->add(w.second == "" ? w.first : w.second, c);
    }
  }
}

void TransformVisitor::visit(const FunctionStmt *stmt) {
  auto canonicalName = ctx->generateCanonicalName(stmt->name);
  bool isClassMember = ctx->getLevel() && ctx->bases.back().isType();

  if (in(stmt->attributes, "builtin") && (ctx->getLevel() || isClassMember))
    error("builtins must be defined at the toplevel");

  generateFunctionStub(stmt->args.size() + 1);
  if (!isClassMember)
    ctx->add(TransformItem::Func, stmt->name, canonicalName, ctx->isToplevel());

  ctx->bases.push_back({canonicalName});
  ctx->addBlock();
  for (auto &g : stmt->generics)
    ctx->add(TransformItem::Type, g.name, "", false, true, g.type != nullptr);

  vector<Param> args;
  for (int ia = 0; ia < stmt->args.size(); ia++) {
    auto &a = stmt->args[ia];
    auto typeAst = transformType(a.type);
    if (!typeAst && isClassMember && ia == 0 && a.name == "self")
      typeAst = transformType(ctx->bases[ctx->bases.size() - 2].ast);
    args.push_back({a.name, move(typeAst), transform(a.deflt)});
    ctx->add(TransformItem::Var, a.name);
  }
  auto ret = transformType(stmt->ret);
  StmtPtr suite = nullptr;
  if (!in(stmt->attributes, "internal") && !in(stmt->attributes, ".c")) {
    ctx->addBlock();
    suite = TransformVisitor(ctx).transform(stmt->suite);
    ctx->popBlock();
  }
  auto referencesParent = isClassMember && ctx->bases.back().referencesParent;
  auto attributes = stmt->attributes;

  if (isClassMember)
    attributes.push_back(".class");
  if (referencesParent ||
      (canonicalName == ".Ptr.__elemsize__" || canonicalName == ".Ptr.__atomic__"))
    attributes.push_back(".method");
  ctx->bases.pop_back();
  ctx->popBlock();

  resultStmt = N<FunctionStmt>(canonicalName, move(ret), clone_nop(stmt->generics),
                               move(args), move(suite), attributes,
                               isClassMember ? ctx->bases.back().name : "");
  ctx->cache->asts[canonicalName] = clone(resultStmt);
}

void TransformVisitor::visit(const ClassStmt *stmt) {
  auto canonicalName = ctx->generateCanonicalName(stmt->name);
  if (ctx->bases.size() && ctx->bases.back().isType())
    error("nested classes are not supported");

  if (!stmt->isRecord)
    ctx->add(TransformItem::Type, stmt->name, canonicalName, ctx->isToplevel());
  ctx->bases.push_back({canonicalName});
  ctx->bases.back().ast = N<IdExpr>(stmt->name);
  if (stmt->generics.size()) {
    vector<ExprPtr> genAst;
    for (auto &g : stmt->generics)
      genAst.push_back(N<IdExpr>(g.name));
    ctx->bases.back().ast =
        N<IndexExpr>(N<IdExpr>(stmt->name), N<TupleExpr>(move(genAst)));
  }

  ctx->addBlock();
  for (auto &g : stmt->generics)
    ctx->add(TransformItem::Type, g.name, "", false, true, g.type != nullptr);
  unordered_set<string> seenMembers;
  ExprPtr mainType = nullptr;
  vector<Param> args;
  for (auto &a : stmt->args) {
    seqassert(a.type, "no type provided for '{}'", a.name);
    if (!mainType)
      mainType = clone(a.type);
    if (seenMembers.find(a.name) != seenMembers.end())
      error(a.type, "'{}' declared twice", a.name);
    seenMembers.insert(a.name);
    args.push_back({a.name, transformType(a.type), nullptr});
  }
  if (!mainType)
    mainType = N<IdExpr>("void");
  if (stmt->isRecord) {
    ctx->popBlock();
    ctx->add(TransformItem::Type, stmt->name, canonicalName, ctx->isToplevel());
    ctx->addBlock();
    for (auto &g : stmt->generics)
      ctx->add(TransformItem::Type, g.name, "", false, true, bool(g.type));
  }

  ctx->cache->asts[canonicalName] =
      N<ClassStmt>(stmt->isRecord, canonicalName, clone_nop(stmt->generics), move(args),
                   N<SuiteStmt>(vector<StmtPtr>()), stmt->attributes);

  auto makeInternal = [&](const auto &name, ExprPtr ret, vector<Param> &&p) {
    auto t = make_unique<FunctionStmt>(name, move(ret), vector<Param>{}, move(p),
                                       nullptr, vector<string>{"internal"});
    t->setSrcInfo(ctx->getGeneratedPos());
    return t;
  };
  vector<StmtPtr> fns;
  if (!in(stmt->attributes, "internal")) {
    ExprPtr codeType = clone(ctx->bases.back().ast);
    vector<Param> args;
    if (!stmt->isRecord)
      args.push_back(Param{"self"});
    for (auto &a : stmt->args)
      args.push_back(Param{a.name, a.type->clone()});
    if (!stmt->isRecord) {
      fns.push_back(makeInternalFn("__new__", clone(codeType)));
      fns.push_back(makeInternal("__init__", N<IdExpr>("void"), move(args)));
      fns.push_back(makeInternalFn("__bool__", N<IdExpr>(".bool"), Param{"self"}));
      fns.push_back(makeInternalFn("__pickle__", N<IdExpr>("void"), Param{"self"},
                                   Param{"dest", N<IdExpr>("cobj")}));
      fns.push_back(makeInternalFn("__unpickle__", clone(codeType),
                                   Param{"src", N<IdExpr>("cobj")}));
      fns.push_back(makeInternalFn("__raw__", N<IdExpr>("cobj"), Param{"self"}));
    } else {
      bool empty = canonicalName == ".Tuple.0";
      fns.push_back(makeInternal("__new__", clone(codeType), move(args)));
      fns.push_back(makeInternalFn("__str__", N<IdExpr>(".str"), Param{"self"}));
      fns.push_back(makeInternalFn("__len__", N<IdExpr>(".int"), Param{"self"}));
      fns.push_back(makeInternalFn("__hash__", N<IdExpr>(".int"), Param{"self"}));
      fns.push_back(makeInternalFn(
          "__iter__", N<IndexExpr>(N<IdExpr>("Generator"), N<IdExpr>(".int")),
          Param{"self"}));
      fns.push_back(makeInternalFn("__pickle__", N<IdExpr>("void"), Param{"self"},
                                   Param{"dest", N<IdExpr>("cobj")}));
      fns.push_back(makeInternalFn("__unpickle__", clone(codeType),
                                   Param{"src", N<IdExpr>("cobj")}));
      fns.push_back(makeInternalFn("__getitem__",
                                   empty ? N<IdExpr>("void") : clone(mainType),
                                   Param{"self"}, Param{"index", N<IdExpr>(".int")}));
      if (!empty)
        fns.push_back(makeInternalFn("__contains__", N<IdExpr>(".bool"), Param{"self"},
                                     Param{"what", clone(mainType)}));
      fns.push_back(makeInternalFn("__to_py__", N<IdExpr>(".pyobj"), Param{"self"}));
      fns.push_back(makeInternalFn("__from_py__", clone(codeType),
                                   Param{"src", N<IdExpr>(".pyobj")}));
      for (auto &m : {"__eq__", "__ne__", "__lt__", "__gt__", "__le__", "__ge__"})
        fns.push_back(makeInternalFn(m, N<IdExpr>(".bool"), Param{"self"},
                                     Param{"what", clone(codeType)}));
    }
  }
  fns.push_back(clone(stmt->suite));
  auto suite = N<SuiteStmt>(vector<StmtPtr>{});
  for (auto &s : fns)
    for (auto &sp : addMethods(s))
      suite->stmts.push_back(move(sp));
  ctx->bases.pop_back();
  ctx->popBlock();

  static_cast<ClassStmt *>(ctx->cache->asts[canonicalName].get())->suite = move(suite);
  resultStmt = clone(ctx->cache->asts[canonicalName]);
}

void TransformVisitor::visit(const ExtendStmt *stmt) {
  ExprPtr type = nullptr;
  vector<string> generics;
  vector<ExprPtr> genericAst;
  if (auto e = CAST(stmt->type, IndexExpr)) {
    type = transformType(e->expr);
    if (auto t = CAST(e->index, TupleExpr)) {
      for (auto &ti : t->items)
        if (auto s = CAST(ti, IdExpr)) {
          generics.push_back(s->value);
          genericAst.push_back(clone(ti));
        } else {
          error(ti, "invalid generic identifier");
        }
    } else if (auto i = CAST(e->index, IdExpr)) {
      generics.push_back(i->value);
      genericAst.push_back(i->clone());
    } else {
      error(e->index, "invalid generic identifier");
    }
  } else {
    type = transformType(stmt->type);
  }
  string canonicalName;
  if (auto i = CAST(type, IdExpr))
    canonicalName = i->value;
  else
    error("'{}' is not a valid type", type->toString());

  const auto &astIter = ctx->cache->asts.find(canonicalName);
  assert(astIter != ctx->cache->asts.end());
  const auto *ast = CAST(astIter->second, ClassStmt);
  assert(ast);

  ctx->bases.push_back({canonicalName});
  ctx->addBlock();
  ctx->bases.back().ast = N<IdExpr>(ctx->cache->reverseLookup[canonicalName]);
  if (genericAst.size())
    ctx->bases.back().ast =
        N<IndexExpr>(N<IdExpr>(ctx->cache->reverseLookup[canonicalName]),
                     N<TupleExpr>(move(genericAst)));
  if (generics.size() != ast->generics.size())
    error("expected {} generics, got {}", ast->generics.size(), generics.size());
  for (int i = 0; i < generics.size(); i++)
    ctx->add(TransformItem::Type, generics[i], "", false, true,
             ast->generics[i].type != nullptr);

  auto fns = addMethods(stmt->suite);
  ctx->bases.pop_back();
  ctx->popBlock();

  auto e = N<ExtendStmt>(N<IdExpr>(canonicalName), N<SuiteStmt>(move(fns)));
  e->generics = generics;
  resultStmt = move(e);
}

void TransformVisitor::visit(const ExternImportStmt *stmt) {
  if (stmt->lang == "c" && stmt->from) {
    vector<StmtPtr> stmts;
    stmts.push_back(N<AssignStmt>(N<IdExpr>(".Ptr"),
                                  N<CallExpr>(N<IdExpr>("_dlsym"), clone(stmt->from),
                                              N<StringExpr>(stmt->name.first))));
    vector<ExprPtr> args;
    args.push_back(stmt->ret ? clone(stmt->ret) : N<IdExpr>("void"));
    for (auto &a : stmt->args)
      args.push_back(clone(a.type));
    stmts.push_back(N<AssignStmt>(
        N<IdExpr>("f"),
        N<CallExpr>(N<IndexExpr>(N<IdExpr>("Function"), N<TupleExpr>(move(args))),
                    N<IdExpr>(".Ptr"))));
    bool isVoid = true;
    if (stmt->ret) {
      if (auto f = CAST(stmt->ret, IdExpr))
        isVoid = f->value == "void";
      else
        isVoid = false;
    }
    args.clear();
    for (int i = 0; i < stmt->args.size(); i++)
      args.push_back(
          N<IdExpr>(stmt->args[i].name != "" ? stmt->args[i].name : format("$a{}", i)));
    auto call = N<CallExpr>(N<IdExpr>("f"), move(args));
    if (!isVoid)
      stmts.push_back(N<ReturnStmt>(move(call)));
    else
      stmts.push_back(N<ExprStmt>(move(call)));
    vector<Param> params;
    for (int i = 0; i < stmt->args.size(); i++)
      params.push_back(
          {stmt->args[i].name != "" ? stmt->args[i].name : format("$a{}", i),
           clone(stmt->args[i].type)});
    resultStmt = transform(
        N<FunctionStmt>(stmt->name.second != "" ? stmt->name.second : stmt->name.first,
                        clone(stmt->ret), vector<Param>(), move(params),
                        N<SuiteStmt>(move(stmts)), vector<string>()));
  } else if (stmt->lang == "c") {
    auto canonicalName = ctx->generateCanonicalName(stmt->name.first);
    if (ctx->getLevel() && ctx->bases.back().isType())
      error("external functions cannot be class methods");
    if (!stmt->ret)
      error("expected return type");
    vector<Param> args;
    vector<TypePtr> argTypes{};
    generateFunctionStub(stmt->args.size() + 1);
    for (int ai = 0; ai < stmt->args.size(); ai++) {
      if (stmt->args[ai].deflt)
        error("default arguments not supported here");
      if (!stmt->args[ai].type)
        error("type for '{}' not specified", stmt->args[ai].name);
      args.push_back(
          {stmt->args[ai].name.empty() ? format(".a{}", ai) : stmt->args[ai].name,
           transformType(stmt->args[ai].type), nullptr});
    }
    ctx->add(TransformItem::Func,
             stmt->name.second != "" ? stmt->name.second : stmt->name.first,
             canonicalName, ctx->isToplevel());
    resultStmt =
        N<FunctionStmt>(canonicalName, transformType(stmt->ret), vector<Param>(),
                        move(args), nullptr, vector<string>{".c"});
    ctx->cache->asts[canonicalName] = clone(resultStmt);
  } else if (stmt->lang == "py") {
    vector<StmtPtr> stmts;
    string from = "";
    if (auto i = CAST(stmt->from, IdExpr))
      from = i->value;
    else
      error("invalid pyimport query");
    auto call = N<CallExpr>(N<DotExpr>(N<IndexExpr>(N<CallExpr>(N<IdExpr>("_py_import"),
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
    if (!isVoid)
      stmts.push_back(N<ReturnStmt>(
          N<CallExpr>(N<DotExpr>(clone(stmt->ret), "__from_py__"), move(call))));
    else
      stmts.push_back(N<ExprStmt>(move(call)));
    vector<Param> params;
    params.push_back({"x", nullptr, nullptr});
    resultStmt = transform(
        N<FunctionStmt>(stmt->name.second != "" ? stmt->name.second : stmt->name.first,
                        clone(stmt->ret), vector<Param>(), move(params),
                        N<SuiteStmt>(move(stmts)), vector<string>{"pyhandle"}));
  } else {
    error("language '{}' not supported", stmt->lang);
  }
}

void TransformVisitor::visit(const PyDefStmt *stmt) {
  vector<string> args;
  for (auto &a : stmt->args)
    args.push_back(a.name);
  string code =
      format("def {}({}):\n{}\n", stmt->name, fmt::join(args, ", "), stmt->code);
  resultStmt = transform(
      N<SuiteStmt>(N<ExprStmt>(N<CallExpr>(N<IdExpr>("_py_exec"), N<StringExpr>(code))),
                   N<ExternImportStmt>(make_pair(stmt->name, ""), N<IdExpr>("__main__"),
                                       clone(stmt->ret), vector<Param>(), "py")));
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

StmtPtr TransformVisitor::makeInternalFn(const string &name, ExprPtr &&ret, Param &&arg,
                                         Param &&arg2) {
  vector<Param> p;
  if (arg.name.size())
    p.push_back(move(arg));
  if (arg2.name.size())
    p.push_back(move(arg2));
  auto t = make_unique<FunctionStmt>(name, move(ret), vector<Param>{}, move(p), nullptr,
                                     vector<string>{"internal"});
  t->setSrcInfo(ctx->getGeneratedPos());
  return t;
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
      args.push_back(Param{format("a{0}", i), N<IdExpr>(format("T{}", i)), nullptr});
    }
    ExprPtr type = N<IdExpr>(typeName);
    if (genericNames.size())
      type = N<IndexExpr>(move(type), N<TupleExpr>(move(genericNames)));

    vector<StmtPtr> fns;
    fns.push_back(
        makeInternalFn("__new__", clone(type), Param{"what", N<IdExpr>("cobj")}));
    fns.push_back(makeInternalFn("__str__", N<IdExpr>("str"), Param{"self"}));

    StmtPtr stmt =
        make_unique<ClassStmt>(true, typeName, move(generics), clone_nop(args),
                               N<SuiteStmt>(move(fns)), vector<string>{"internal"});
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
      vector<ExprPtr> genericNames;
      for (int i = 1; i <= len_i; i++) {
        genericNames.push_back(N<IdExpr>(format("T{}", i)));
        generics.push_back(Param{format("T{}", i), nullptr, nullptr});
        args.push_back(Param{format("a{0}", i), N<IdExpr>(format("T{}", i)), nullptr});
      }
      ExprPtr type = N<IdExpr>(typeName);
      if (genericNames.size())
        type = N<IndexExpr>(move(type), N<TupleExpr>(move(genericNames)));
      string code = "def __str__(self) -> str:\n";
      code += len_i ? "  s = '('\n" : "  s = '()'\n";
      for (int i = 0; i < len_i; i++) {
        code += format("  s += self[{}].__str__()\n", i);
        code += format("  s += '{}'\n", i == len_i - 1 ? ")" : ", ");
      }
      code += "  return s\n";
      auto fns = parseCode(ctx->getFilename(), code);
      StmtPtr stmt = make_unique<ClassStmt>(true, typeName, move(generics), move(args),
                                            move(fns), vector<string>{});
      stmt->setSrcInfo(ctx->getGeneratedPos());
      TransformVisitor(make_shared<TransformContext>("<generated>", ctx->cache))
          .transform(stmt);
    }
  }
  return fmt::format(".Tuple.{}", len);
}

StmtPtr TransformVisitor::getGeneratorBlock(const vector<GeneratorExpr::Body> &loops,
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
  } else if (!CAST(s, FunctionStmt)) {
    error(s, "expected a function (only functions are allowed within type "
             "definitions)");
  } else {
    v.push_back(transform(s));
  }
  return v;
}

} // namespace ast
} // namespace seq
