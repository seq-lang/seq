/*
 * typecheck_expr.cpp --- Type inference for AST expressions.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "parser/ast.h"
#include "parser/common.h"
#include "parser/visitors/simplify/simplify.h"
#include "parser/visitors/typecheck/typecheck.h"

using fmt::format;
using std::deque;
using std::dynamic_pointer_cast;
using std::get;
using std::move;
using std::ostream;
using std::stack;
using std::static_pointer_cast;

namespace seq {
namespace ast {

using namespace types;

types::TypePtr TypecheckVisitor::realizeFunc(types::TypePtr tt) {
  auto t = tt->getFunc();
  assert(t && t->canRealize());
  try {
    auto it = ctx->cache->functions[t->name].realizations.find(t->realizeString());
    if (it != ctx->cache->functions[t->name].realizations.end()) {
      forceUnify(t, it->second.type);
      return it->second.type;
    }

    int depth = 1;
    for (auto p = t->parent; p;) {
      if (auto f = p->getFunc()) {
        depth++;
        p = f->parent;
      } else {
        p = p->getClass()->parent;
      }
    }
    auto oldBases = vector<TypeContext::RealizationBase>(ctx->bases.begin() + depth,
                                                         ctx->bases.end());
    while (ctx->bases.size() > depth)
      ctx->bases.pop_back();

    if (startswith(t->name, "Tuple.N") &&
        (endswith(t->name, ".__iter__") || endswith(t->name, ".__getitem__"))) {
      auto u = t->args[1]->getClass();
      string s;
      for (auto &a : u->args) {
        if (s.empty())
          s = a->realizeString();
        else if (s != a->realizeString())
          error("cannot iterate a heterogenous tuple");
      }
    }

    LOG_TYPECHECK("[realize] fn {} -> {} : base {} ; depth = {}", t->name,
                  t->realizeString(), ctx->getBase(), depth);
    ctx->addBlock();
    ctx->typecheckLevel++;
    ctx->bases.push_back({t->name, t, t->args[0]});
    auto *ast = ctx->cache->functions[t->name].ast.get();
    addFunctionGenerics(t);
    // There is no AST linked to internal functions, so just ignore them
    bool isInternal = in(ast->attributes, ATTR_INTERNAL);
    isInternal |= ast->suite == nullptr;
    if (!isInternal)
      for (int i = 1; i < t->args.size(); i++) {
        assert(t->args[i] && !t->args[i]->getUnbounds().size());
        ctx->add(TypecheckItem::Var, ast->args[i - 1].name,
                 make_shared<LinkType>(t->args[i]));
      }

    // Need to populate funcRealization in advance to make recursive functions
    // viable
    ctx->cache->functions[t->name].realizations[t->realizeString()] = {t, nullptr};
    ctx->bases[0].visitedAsts[t->realizeString()] = {TypecheckItem::Func,
                                                     t}; // realizations go to the top
    // ctx->getRealizations()->realizationLookup[t->realizeString()] = name;

    StmtPtr realized = nullptr;
    if (!isInternal) {
      ctx->typecheckLevel++;
      auto oldIter = ctx->iteration;
      ctx->iteration = 0;
      realized = realizeBlock(ast->suite);
      ctx->iteration = oldIter;
      ctx->typecheckLevel--;

      if (!ast->ret && t->args[0]->getUnbound())
        forceUnify(t->args[0], ctx->findInternal("void"));
      // if (stmt->)
      // forceUnify(t->args[0], ctx->bases.back().returnType ?
      // ctx->bases.back().returnType : ctx->findInternal(".void"));
    }
    assert(t->args[0]->getClass() && t->args[0]->getClass()->canRealize());
    realizeType(t->args[0]->getClass());
    assert(ast->args.size() == t->args.size() - 1);
    vector<Param> args;
    for (auto &i : ast->args)
      args.emplace_back(Param{i.name, nullptr, nullptr});

    LOG_REALIZE("done with {}", t->realizeString());
    ctx->cache->functions[t->name].realizations[t->realizeString()].ast =
        Nx<FunctionStmt>(ast, t->realizeString(), nullptr, vector<Param>(), move(args),
                         move(realized), map<string, string>(ast->attributes));
    ctx->bases.pop_back();
    ctx->popBlock();
    ctx->typecheckLevel--;

    ctx->bases.insert(ctx->bases.end(), oldBases.begin(), oldBases.end());

    return t;
  } catch (exc::ParserException &e) {
    e.trackRealize(fmt::format("{} (arguments {})", t->name, t->toString()),
                   getSrcInfo());
    throw;
  }
}

types::TypePtr TypecheckVisitor::realizeType(types::TypePtr tt) {
  auto t = tt->getClass();
  assert(t && t->canRealize());
  types::TypePtr ret = nullptr;
  try {
    auto it = ctx->cache->classes[t->name].realizations.find(t->realizeString());
    if (it != ctx->cache->classes[t->name].realizations.end()) {
      ret = it->second.type;
      goto end;
    }

    LOG_REALIZE("[realize] ty {} -> {}", t->name, t->realizeString());
    ctx->bases[0].visitedAsts[t->realizeString()] = {TypecheckItem::Type,
                                                     t}; // realizations go to the top
    ctx->cache->classes[t->name].realizations[t->realizeString()] = {t, {}};
    ret = t;
    goto end;
  } catch (exc::ParserException &e) {
    e.trackRealize(t->toString(), getSrcInfo());
    throw;
  }
end:
  // Check if all members are initialized
  if (ctx->cache->classes[t->name].realizations[t->realizeString()].fields.empty()) {
    for (auto &m : ctx->cache->classes[t->name].fields)
      if (!m.type)
        return ret;
    for (auto &m : ctx->cache->classes[t->name].fields) {
      auto mt = ctx->instantiate(t->getSrcInfo(), m.type, t.get());
      LOG_REALIZE("- member: {} -> {}: {}", m.name, m.type->toString(), mt->toString());
      assert(mt->getClass() && mt->getClass()->canRealize());
      ctx->cache->classes[t->name].realizations[t->realizeString()].fields.emplace_back(
          m.name, realizeType(mt->getClass()));
    }
  }
  return ret;
}

StmtPtr TypecheckVisitor::realizeBlock(const StmtPtr &stmt, bool keepLast) {
  if (!stmt)
    return nullptr;
  StmtPtr result = nullptr;

  // We keep running typecheck transformations until there are no more unbound
  // types. It is assumed that the unbound count will decrease in each
  // iteration--- if not, the program cannot be type-checked.
  // TODO: this can be probably optimized one day...
  int minUnbound = ctx->cache->unboundCount;
  for (int iter = 0, prevSize = INT_MAX;; iter++, ctx->iteration++) {
    ctx->addBlock();
    if (keepLast) // reset extendCount in whole code loop
      ctx->extendEtape = 0;
    result = TypecheckVisitor(ctx).transform(result ? result : stmt);

    int newUnbounds = 0;
    set<types::TypePtr> newActiveUnbounds;
    for (auto i = ctx->activeUnbounds.begin(); i != ctx->activeUnbounds.end();) {
      auto l = (*i)->getLink();
      assert(l);
      if (l->kind == LinkType::Unbound) {
        newActiveUnbounds.insert(*i);
        if (l->id >= minUnbound)
          newUnbounds++;
      }
      ++i;
    }
    ctx->activeUnbounds = newActiveUnbounds;

    ctx->popBlock();
    if (ctx->activeUnbounds.empty() || !newUnbounds) {
      break;
    } else {

      if (newUnbounds >= prevSize) {
        TypePtr fu = nullptr;
        int count = 0;
        for (auto &ub : ctx->activeUnbounds)
          if (ub->getLink()->id >= minUnbound) {
            // Attempt to use default generics here
            // TODO: this is awfully inefficient way to do it
            // if (ctx->...)
            if (!fu)
              fu = ub;
            LOG_TYPECHECK("[realizeBlock] dangling {} @ {}", ub->toString(),
                          ub->getSrcInfo());
            count++;
          }
        error(fu, "cannot resolve {} unbound variables", count);
      }
      prevSize = newUnbounds;
    }
    LOG_TYPECHECK("=========================== {}",
                  ctx->bases.back().type ? ctx->bases.back().type->toString() : "-");
  }
  // Last pass; TODO: detect if it is needed...
  ctx->addBlock();
  LOG_TYPECHECK("=========================== {}",
                ctx->bases.back().type ? ctx->bases.back().type->toString() : "-");
  result = TypecheckVisitor(ctx).transform(result);
  if (!keepLast)
    ctx->popBlock();
  return result;
}

} // namespace ast
} // namespace seq