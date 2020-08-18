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
#include "parser/ast/transform_ctx.h"
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

namespace seq {
namespace ast {

using namespace types;

void TransformVisitor::prepend(StmtPtr s) {
  if (auto t = transform(s))
    prependStmts->push_back(move(t));
}

ExprPtr TransformVisitor::conditionalMagic(const ExprPtr &expr,
                                           const string &type,
                                           const string &magic) {
  auto e = transform(expr);
  if (!ctx->isTypeChecking())
    return e;
  if (e->getType()->getUnbound())
    return e;
  if (auto c = e->getType()->getClass()) {
    if (chop(c->name) == type)
      return e;
    return transform(
        Nx<CallExpr>(e.get(), Nx<DotExpr>(e.get(), move(e), magic)));
  } else {
    error(e, "cannot find magic '{}' in {}", magic, e->getType()->toString());
  }
  return nullptr;
}

ExprPtr TransformVisitor::makeBoolExpr(const ExprPtr &e) {
  return conditionalMagic(e, "bool", "__bool__");
}

shared_ptr<TypeItem::Item>
TransformVisitor::processIdentifier(shared_ptr<TypeContext> tctx,
                                    const string &id) {
  auto val = tctx->find(id);
  if (!val)
    return nullptr;
  return val;
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

string TransformVisitor::patchIfRealizable(TypePtr typ, bool isClass) {
  // Patch the name if it can be realized
  if (typ->canRealize()) {
    if (isClass) {
      auto r = realizeType(typ->getClass());
      forceUnify(typ, r.type);
      return r.fullName;
    } else if (typ->getFunc()) {
      auto r = realizeFunc(typ->getFunc());
      forceUnify(typ, r.type);
      return r.fullName;
    }
  }
  return "";
}

void TransformVisitor::fixExprName(ExprPtr &e, const string &newName) {
  if (auto i = CAST(e, CallExpr)) // partial calls
    fixExprName(i->expr, newName);
  else if (auto i = CAST(e, IdExpr))
    i->value = newName;
  else if (auto i = CAST(e, DotExpr))
    i->member = newName;
  else {
    LOG7("fixing {}", *e);
    assert(false);
  }
}

StmtPtr TransformVisitor::makeInternalFn(const string &name, ExprPtr &&ret,
                                         Param &&arg, Param &&arg2) {
  vector<Param> p;
  if (arg.name.size())
    p.push_back(move(arg));
  if (arg2.name.size())
    p.push_back(move(arg2));
  auto t = make_unique<FunctionStmt>(name, move(ret), vector<Param>{}, move(p),
                                     nullptr, vector<string>{"internal"});
  t->setSrcInfo(ctx->getRealizations()->getGeneratedPos());
  return t;
}
StmtPtr TransformVisitor::makeInternalFn(const string &name, ExprPtr &&ret,
                                         vector<Param> &&p) {
  auto t = make_unique<FunctionStmt>(name, move(ret), vector<Param>{}, move(p),
                                     nullptr, vector<string>{"internal"});
  t->setSrcInfo(ctx->getRealizations()->getGeneratedPos());
  return t;
}

string TransformVisitor::generateVariardicStub(const string &name, int len) {
  // TODO: handle name clashes (add special name character?)
  auto typeName = fmt::format("{}.{}", name, len);
  // LOG("{}", typeName);
  if (ctx->getRealizations()->variardicCache.find(typeName) ==
      ctx->getRealizations()->variardicCache.end()) {
    if (name != "tuple")
      assert(len >= 1);
    ctx->getRealizations()->variardicCache.insert(typeName);
    vector<Param> generics, args;
    vector<ExprPtr> genericNames;
    for (int i = 1; i <= len; i++) {
      genericNames.push_back(N<IdExpr>(format("T{}", i)));
      generics.push_back(Param{format("T{}", i), nullptr, nullptr});
      args.push_back(
          Param{format("a{0}", i), N<IdExpr>(format("T{}", i)), nullptr});
    }
    ExprPtr type = N<IdExpr>(typeName);
    if (genericNames.size())
      type = N<IndexExpr>(move(type), N<TupleExpr>(move(genericNames)));
    auto stmt = make_unique<ClassStmt>(true, typeName, move(generics),
                                       move(args), nullptr, vector<string>{});
    stmt->setSrcInfo(ctx->getRealizations()->getGeneratedPos());

    vector<StmtPtr> fns;
    if (name == "function") {
      fns.push_back(makeInternalFn("__new__", type->clone(),
                                   Param{"what", N<IdExpr>("cobj")}));
      fns.push_back(makeInternalFn("__str__", N<IdExpr>("str"), Param{"self"}));
      stmt->attributes.push_back("internal");
    } else if (name == "tuple") {
      string code = "def __str__(self) -> str:\n";
      code += len ? "  s = '('\n" : "  s = '()'\n";
      for (int i = 0; i < len; i++) {
        code += format("  s += self[{}].__str__()\n", i);
        code += format("  s += '{}'\n", i == len - 1 ? ")" : ", ");
      }
      code += "  return s\n";
      fns.push_back(
          parseCode(ctx->getFilename(), code)->getStatements()[0]->clone());
    } else if (name != "partial") {
      error("invalid variardic type");
    }
    // LOG7("[VAR] generating {}...\n{}", typeName, code);
    stmt->suite = N<SuiteStmt>(move(fns));
    auto i = ctx->getImports()->getImport("");
    auto stmtPtr = dynamic_cast<SuiteStmt *>(i->statements.get());
    assert(stmtPtr);

    // TODO: move to stdlib?
    auto nc = make_shared<TypeContext>(i->tctx->getFilename(),
                                       i->tctx->getRealizations(),
                                       i->tctx->getImports());
    stmtPtr->stmts.push_back(TransformVisitor(nc).transform(stmt));
    for (auto &ax : *nc)
      i->tctx->addToplevel(ax.first, ax.second.front());
  }
  return typeName;
}

FuncTypePtr
TransformVisitor::findBestCall(ClassTypePtr c, const string &member,
                               const vector<pair<string, TypePtr>> &args,
                               bool failOnMultiple, TypePtr retType) {
  auto m = ctx->getRealizations()->findMethod(c->name, member);
  if (!m)
    return nullptr;

  if (m->size() == 1) // works
    return (*m)[0];

  // TODO: For now, overloaded functions are only possible in magic methods
  // Another assomption is that magic methods of interest have no default
  // arguments or reordered arguments...
  if (member.substr(0, 2) != "__" || member.substr(member.size() - 2) != "__") {
    error("overloaded non-magic method...");
  }
  for (auto &a : args)
    if (!a.first.empty())
      error("[todo] named magic call");

  vector<pair<int, int>> scores;
  for (int i = 0; i < m->size(); i++) {
    auto mt = dynamic_pointer_cast<FuncType>(
        ctx->instantiate(getSrcInfo(), (*m)[i], c, false));
    auto s = 0;
    if (mt->args.size() - 1 != args.size())
      continue;
    for (int j = 0; j < args.size(); j++) {
      Unification us;
      int u = args[j].second->unify(mt->args[j + 1], us);
      us.undo();
      if (u < 0) {
        s = -1;
        break;
      } else {
        s += u;
      }
    }
    if (retType) {
      Unification us;
      int u = retType->unify(mt->args[0], us);
      us.undo();
      s = u < 0 ? -1 : s + u;
    }
    if (s >= 0)
      scores.push_back({s, i});
  }
  if (!scores.size())
    return nullptr;
  sort(scores.begin(), scores.end(), std::greater<pair<int, int>>());
  if (failOnMultiple) {
    // for (int i = 1; i < scores.size(); i++)
    //   if (scores[i].first == scores[0].first)
    //     // return nullptr;
    //     compilationWarning(format("multiple choices for magic call, selected
    //     {}",
    //                               (*m)[scores[0].second]->canonicalName),
    //                        getSrcInfo().file, getSrcInfo().line);
    //   else
    //     break;
  }
  return (*m)[scores[0].second];
}

// vector<int> TransformVisitor::callCallable(types::TypePtr f,
//                                            vector<CallExpr::Arg> &args,
//                                            vector<CallExpr::Arg>
//                                            &reorderedArgs) {
//   assert(f->getCallable());
//   bool isPartial = false;
//   if (args.size() != f->args.size() - 1) {
//     if (args.size() == f->args.size() && CAST(args.back().value,
//     EllipsisExpr)) {
//       isPartial = true;
//       args.pop_back();
//     } else {
//       error("too many arguments for {} (expected {}, got {})", f->toString(),
//             f->args.size() - 1, args.size());
//     }
//   }
//   vector<int> pending;
//   for (int i = 0; i < args.size(); i++) {
//     if (args[i].name != "")
//       error("argument '{}' missing (function pointers have argument "
//             "names elided)",
//             args[i].name);
//     reorderedArgs.push_back({"", move(args[i].value)});

//     forceUnify(reorderedArgs[i].value, f->args[i + 1]);
//     if (CAST(reorderedArgs[i].value, EllipsisExpr))
//       pending.push_back(i);
//   }
//   if (isPartial || pending.size())
//     pending.push_back(args.size());
//   return pending;
// }

vector<int> TransformVisitor::callFunc(types::ClassTypePtr f,
                                       vector<CallExpr::Arg> &args,
                                       vector<CallExpr::Arg> &reorderedArgs,
                                       const vector<int> &availableArguments) {
  vector<int> pending;
  bool isPartial = false;
  bool namesStarted = false;
  unordered_map<string, ExprPtr> namedArgs;
  for (int i = 0; i < args.size(); i++) {
    if (args[i].name == "" && namesStarted)
      error("unnamed argument after a named argument");
    namesStarted |= args[i].name != "";
    if (args[i].name == "")
      reorderedArgs.push_back({"", move(args[i].value)});
    else if (namedArgs.find(args[i].name) == namedArgs.end())
      namedArgs[args[i].name] = move(args[i].value);
    else
      error("named argument {} repeated multiple times", args[i].name);
  }

  if (namedArgs.size() == 0 &&
      reorderedArgs.size() == availableArguments.size() + 1 &&
      CAST(reorderedArgs.back().value, EllipsisExpr)) {
    isPartial = true;
    reorderedArgs.pop_back();
  } else if (reorderedArgs.size() + namedArgs.size() >
             availableArguments.size()) {
    error("too many arguments for {} (expected {}, got {})", f->toString(),
          availableArguments.size(), reorderedArgs.size() + namedArgs.size());
  }

  FunctionStmt *ast = nullptr;
  if (f->getFunc()) {
    ast = dynamic_cast<FunctionStmt *>(
        ctx->getRealizations()->getAST(f->getFunc()->canonicalName).get());
    assert(ast);
  }
  if (!ast && namedArgs.size())
    error("unexpected name '{}' (function pointers have argument "
          "names elided)",
          namedArgs.begin()->first);
  for (int i = 0, ra = reorderedArgs.size(); i < availableArguments.size();
       i++) {
    if (i >= ra) {
      assert(ast);
      auto it = namedArgs.find(ast->args[availableArguments[i]].name);
      if (it != namedArgs.end()) {
        reorderedArgs.push_back({"", move(it->second)});
        namedArgs.erase(it);
      } else if (ast->args[i].deflt) {
        reorderedArgs.push_back(
            {"", transform(ast->args[availableArguments[i]].deflt)});
      } else {
        error("argument '{}' missing", ast->args[availableArguments[i]].name);
      }
    }
    if (CAST(reorderedArgs[i].value, EllipsisExpr))
      pending.push_back(availableArguments[i]);
    if (!wrapOptional(f->args[availableArguments[i] + 1],
                      reorderedArgs[i].value))
      forceUnify(reorderedArgs[i].value, f->args[availableArguments[i] + 1]);
  }
  for (auto &i : namedArgs)
    error(i.second, "unknown argument {}", i.first);
  if (isPartial || pending.size())
    pending.push_back(args.size());
  return pending;
}

// vector<int> TransformVisitor::callPartial(types::PartialTypePtr f,
//                                           vector<CallExpr::Arg> &args,
//                                           vector<CallExpr::Arg>
//                                           &reorderedArgs) {
//   // TODO: parse named arguments for partial functions
//   bool isPartial = false;
//   if (args.size() != f->args.size() - 1 - f->knownTypes.size()) {
//     if (args.size() == f->args.size() - 1 - f->knownTypes.size() + 1 &&
//         CAST(args.back().value, EllipsisExpr)) {
//       isPartial = true;
//       args.pop_back();
//     } else {
//       error("too many arguments for {} (expected {}, got {})", f->toString(),
//             f->knownTypes.size(), args.size());
//     }
//   }
//   vector<int> pending;
//   for (int i = 0; i < args.size(); i++) {
//     if (args[i].name != "")
//       error("argument '{}' missing (partial calls have argument "
//             "names elided)",
//             args[i].name);
//     reorderedArgs.push_back({"", move(args[i].value)});
//     forceUnify(reorderedArgs[i].value, f->args[f->knownTypes[i] + 1]);
//     if (CAST(reorderedArgs[i].value, EllipsisExpr))
//       pending.push_back(f->knownTypes[i]);
//   }
//   if (isPartial || pending.size())
//     pending.push_back(args.size());
//   return pending;
// }

bool TransformVisitor::handleStackAlloc(const CallExpr *expr) {
  if (auto ix = CAST(expr->expr, IndexExpr)) {
    if (auto id = CAST(ix->expr, IdExpr)) {
      if (id->value == "__array__") {
        if (expr->args.size() != 1)
          error("__array__ requires only size argument");
        resultExpr = transform(N<StackAllocExpr>(ix->index->clone(),
                                                 expr->args[0].value->clone()));
        return true;
      }
    }
  }
  return false;
}

bool TransformVisitor::wrapOptional(TypePtr lt, ExprPtr &rhs) {
  auto lc = lt->getClass();
  auto rc = rhs->getType()->getClass();
  if (lc && lc->name == "optional" && rc && rc->name != "optional") {
    rhs = transform(Nx<CallExpr>(rhs.get(), Nx<IdExpr>(rhs.get(), "optional"),
                                 rhs->clone()));
    forceUnify(lc, rhs->getType());
    return true;
  }
  return false;
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
    return transform(
        Nx<AssignMemberStmt>(lhs, l->expr->clone(), l->member, rhs->clone()));
  } else if (auto l = dynamic_cast<const IdExpr *>(lhs)) {
    auto typExpr = transform(type, true);
    if (ctx->isTypeChecking() && typExpr && !typExpr->isType())
      error(typExpr, "expected type expression");

    TypePtr typ = typExpr ? typExpr->getType() : nullptr;
    auto s = Nx<AssignStmt>(lhs, l->clone(), transform(rhs, true),
                            move(typExpr), false, force);
    auto val = processIdentifier(ctx, l->value);
    if (!force && !typ && val && val->getVar() &&
        val->getModule() == ctx->getFilename() &&
        val->getBase() == ctx->getBase()) {
      if (ctx->isTypeChecking() && !wrapOptional(val->getType(), s->rhs))
        s->lhs->setType(forceUnify(s->rhs.get(), val->getType()));
      return Nx<UpdateStmt>(lhs, move(s->lhs), move(s->rhs));
    }

    if (ctx->isTypeChecking()) {
      if (typ && typ->getClass()) {
        if (!wrapOptional(typ, s->rhs))
          forceUnify(typ, s->rhs->getType());
      }
      s->lhs->setType(s->rhs->getType());
      if (s->rhs->isType())
        ctx->addType(l->value, s->rhs->getType());
      else if (dynamic_pointer_cast<FuncType>(s->rhs->getType()))
        ctx->addFunc(l->value, s->rhs->getType());
      else
        ctx->addVar(l->value, s->rhs->getType());
    } else {
      // Add dummy for now!
      ctx->addVar(l->value, nullptr);
    }
    return s;
  } else {
    error("invalid assignment");
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
        error(lefts[st], "multiple unpack expressions found");
      processAssignment(
          lefts[st],
          Nx<IndexExpr>(rhs, rhs->clone(), Nx<IntExpr>(rhs, -lefts.size() + st))
              .release(),
          stmts, force);
    }
  }
}

vector<types::Generic>
TransformVisitor::parseGenerics(const vector<Param> &generics) {
  auto genericTypes = vector<types::Generic>();
  for (auto &g : generics) {
    assert(!g.name.empty());
    if (g.type && g.type->toString() != "(#id int)")
      error("only int generic types are allowed");
    genericTypes.push_back(
        {g.name,
         make_shared<LinkType>(LinkType::Generic,
                               ctx->getRealizations()->getUnboundCount(), 0,
                               nullptr, bool(g.type)),
         ctx->getRealizations()->getUnboundCount()});
    auto tp = make_shared<LinkType>(LinkType::Unbound,
                                    ctx->getRealizations()->getUnboundCount(),
                                    ctx->getLevel(), nullptr, bool(g.type));
    if (g.type)
      ctx->addStatic(g.name, 0, tp);
    else
      ctx->addType(g.name, tp);
    ctx->getRealizations()->getUnboundCount()++;
  }
  return genericTypes;
}

StmtPtr TransformVisitor::addMethod(Stmt *s, const string &canonicalName) {
  if (auto f = dynamic_cast<FunctionStmt *>(s)) {
    auto fs = transform(f);
    auto name = ctx->getRealizations()->getCanonicalName(f->getSrcInfo());
    auto val = ctx->find(name);
    assert(val);
    auto fv = val->getType()->getFunc();
    LOG9("[add_method] {} ... {}", name, val->getType()->toString());
    assert(fv);
    ctx->getRealizations()->classes[canonicalName].methods[f->name].push_back(
        fv);
    return fs;
  } else {
    error(s, "expected a function (only functions are allowed within type "
             "definitions)");
    return nullptr;
  }
}

int TransformVisitor::realizeStatic(StaticTypePtr st) {
  assert(st->canRealize());
  return st->getValue();
}

RealizationContext::FuncRealization
TransformVisitor::realizeFunc(FuncTypePtr t) {
  assert(t->canRealize());
  try {
    auto ret = t->args[0];
    auto name = t->canonicalName;
    auto it = ctx->getRealizations()->funcRealizations.find(name);
    if (it != ctx->getRealizations()->funcRealizations.end()) {
      auto it2 = it->second.find(t->realizeString());
      if (it2 != it->second.end())
        return it2->second;
    }

    LOG3("realizing fn {} -> {}", name, t->realizeString());
    ctx->addBlock();
    ctx->increaseLevel();
    assert(ctx->getRealizations()->funcASTs.find(name) !=
           ctx->getRealizations()->funcASTs.end());
    auto &ast = ctx->getRealizations()->funcASTs[name];
    ctx->pushBase(ast.second->name);
    // Ensure that all inputs are realized
    for (auto p = t->parent; p; p = p->parent)
      for (auto &g : p->explicits)
        if (auto s = g.type->getStatic())
          ctx->addStatic(g.name, 0, s);
        else if (!g.name.empty())
          ctx->addType(g.name, g.type);
    for (auto &g : t->explicits)
      if (auto s = g.type->getStatic())
        ctx->addStatic(g.name, 0, s);
      else if (!g.name.empty())
        ctx->addType(g.name, g.type);
    // There is no AST linked to internal functions, so just ignore them
    bool isInternal = in(ast.second->attributes, "internal");
    isInternal |= ast.second->suite == nullptr;
    if (!isInternal)
      for (int i = 1; i < t->args.size(); i++) {
        assert(t->args[i] && !t->args[i]->hasUnbound());
        ctx->addVar(ast.second->args[i - 1].name,
                    make_shared<LinkType>(t->args[i]));
      }
    auto old = ctx->getReturnType();
    auto oldSeen = ctx->wasReturnSet();
    ctx->setReturnType(ret);
    ctx->setWasReturnSet(false);
    ctx->addBaseType(t);

    // __level__++;

    // Need to populate funcRealization in advance to make recursive functions
    // viable
    auto &result =
        ctx->getRealizations()->funcRealizations[name][t->realizeString()] = {
            t->realizeString(), t, nullptr, nullptr, ctx->getBase()};
    ctx->getRealizations()->realizationLookup[t->realizeString()] = name;

    auto realized =
        isInternal ? nullptr : realizeBlock(ast.second->suite.get());
    // __level__--;
    ctx->popBase();
    ctx->popBaseType();

    if (realized && !ctx->wasReturnSet() && ret)
      forceUnify(ctx->getReturnType(), ctx->findInternal("void"));
    assert(ret->canRealize() && ret->getClass());
    realizeType(ret->getClass());

    assert(ast.second->args.size() == t->args.size() - 1);
    vector<Param> args;
    for (auto &i : ast.second->args)
      args.push_back({i.name, nullptr, nullptr});
    LOG7("realized fn {} -> {}", name, t->realizeString());
    result.ast = Nx<FunctionStmt>(ast.second.get(), ast.second->name, nullptr,
                                  vector<Param>(), move(args), move(realized),
                                  ast.second->attributes);
    ctx->setReturnType(old);
    ctx->setWasReturnSet(oldSeen);
    ctx->decreaseLevel();
    ctx->popBlock();
    // ctx->addRealization(t);
    return result;
  } catch (exc::ParserException &e) {
    e.trackRealize(
        fmt::format("{} (arguments {})", t->canonicalName, t->toString(1)),
        getSrcInfo());
    throw;
  }
}

RealizationContext::ClassRealization
TransformVisitor::realizeType(ClassTypePtr t) {
  assert(t && t->canRealize());
  try {
    auto rs = t->realizeString(
        t->name, false); // necessary for generating __function stubs
    auto it = ctx->getRealizations()->classRealizations.find(t->name);
    if (it != ctx->getRealizations()->classRealizations.end()) {
      auto it2 = it->second.find(rs);
      if (it2 != it->second.end())
        return it2->second;
    }

    LOG7("realizing ty {} -> {}", t->name, rs);
    vector<pair<string, ClassTypePtr>> args;
    for (auto &m : ctx->getRealizations()->classes[t->name].members) {
      auto mt = ctx->instantiate(t->getSrcInfo(), m.second, t);
      assert(mt->canRealize() && mt->getClass());
      args.push_back(make_pair(m.first, realizeType(mt->getClass()).type));
    }
    ctx->getRealizations()->realizationLookup[rs] = t->name;
    // ctx->addRealization(t);
    return ctx->getRealizations()->classRealizations[t->name][rs] = {
               rs, t, args, nullptr, ctx->getBase()};
  } catch (exc::ParserException &e) {
    e.trackRealize(t->toString(), getSrcInfo());
    throw;
  }
}

StmtPtr TransformVisitor::realizeBlock(const Stmt *stmt, bool keepLast) {
  if (!stmt)
    return nullptr;
  StmtPtr result = nullptr;

  // We keep running typecheck transformations until there are no more unbound
  // types. It is assumed that the unbound count will decrease in each
  // iteration--- if not, the program cannot be type-checked.
  // TODO: this can be probably optimized one day...
  int minUnbound = ctx->getRealizations()->unboundCount;
  for (int iter = 0, prevSize = INT_MAX;; iter++) {
    ctx->addBlock();
    TransformVisitor v(ctx);
    result = v.transform(result ? result.get() : stmt);

    int newUnbounds = 0;
    for (auto i = ctx->getActiveUnbounds().begin();
         i != ctx->getActiveUnbounds().end();) {
      auto l = (*i)->getLink();
      assert(l);
      if (l->kind != LinkType::Unbound) {
        i = ctx->getActiveUnbounds().erase(i);
        continue;
      }
      if (l->id >= minUnbound)
        newUnbounds++;
      ++i;
    }

    ctx->popBlock();
    if (ctx->getActiveUnbounds().empty() || !newUnbounds) {
      break;
    } else {
      if (newUnbounds >= prevSize) {
        TypePtr fu = nullptr;
        for (auto &ub : ctx->getActiveUnbounds())
          if (ub->getLink()->id >= minUnbound) {
            if (!fu)
              fu = ub;
            LOG7("NOPE {} @ {}", ub->toString(), ub->getSrcInfo());
          }
        error(fu, "cannot resolve unbound variables");
      }
      prevSize = newUnbounds;
    }
  }
  // Last pass; TODO: detect if it is needed...
  ctx->addBlock();
  TransformVisitor v(ctx);
  result = v.transform(result);
  if (!keepLast)
    ctx->popBlock();
  return result;
}

} // namespace ast
} // namespace seq
