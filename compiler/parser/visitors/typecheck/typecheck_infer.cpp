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

types::TypePtr TypecheckVisitor::realizeType(const types::TypePtr &typ) {
  auto type = typ->getClass();
  seqassert(type && type->canRealize(), "type {} not realizable", typ->toString());

  auto realizeFields = [&](const shared_ptr<ClassType> &type) {
    if (ctx->cache->classes[type->name]
            .realizations[type->realizeString()]
            .fields.empty()) {
      for (auto &m : ctx->cache->classes[type->name].fields)
        if (!m.type)
          return type;
      for (auto &m : ctx->cache->classes[type->name].fields) {
        auto mt = ctx->instantiate(type->getSrcInfo(), m.type, type.get());
        LOG_REALIZE("- member: {} -> {}: {}", m.name, m.type->toString(),
                    mt->toString());
        seqassert(mt->getClass() && mt->getClass()->canRealize(),
                  "cannot realize {}.{}", typ->realizeString(), m.name);
        ctx->cache->classes[type->name]
            .realizations[type->realizeString()]
            .fields.emplace_back(m.name, realizeType(mt->getClass()));
      }
    }
    return type;
  };
  try {
    types::TypePtr realizedType = nullptr;
    auto it = ctx->cache->classes[type->name].realizations.find(type->realizeString());
    if (it != ctx->cache->classes[type->name].realizations.end()) {
      realizedType = it->second.type;
    } else {
      LOG_REALIZE("[realize] ty {} -> {}", type->name, type->realizeString());
      // Realizations are stored in the top-most base.
      ctx->bases[0].visitedAsts[type->realizeString()] = {TypecheckItem::Type, type};
      ctx->cache->classes[type->name].realizations[type->realizeString()] = {type, {}};
      realizedType = type;
    }
    return realizeFields(realizedType->getClass());
  } catch (exc::ParserException &e) {
    e.trackRealize(type->toString(), getSrcInfo());
    throw;
  }
}

types::TypePtr TypecheckVisitor::realizeFunc(const types::TypePtr &typ) {
  auto type = typ->getFunc();
  seqassert(type && type->canRealize(), "type {} not realizable", typ->toString());

  try {
    auto it =
        ctx->cache->functions[type->name].realizations.find(type->realizeString());
    if (it != ctx->cache->functions[type->name].realizations.end())
      return it->second.type;

    // Set up bases. Ensure that we have proper parent bases even during a realization
    // of mutually recursive functions.
    int depth = 1;
    for (auto p = type->parent; p;) {
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

    // Special cases: Tuple.__iter__ and Tuple.__getitem__.
    if (startswith(type->name, "Tuple.N") &&
        (endswith(type->name, ".__iter__") || endswith(type->name, ".__getitem__"))) {
      auto u = type->args[1]->getClass();
      string s;
      for (auto &a : u->args) {
        if (s.empty())
          s = a->realizeString();
        else if (s != a->realizeString())
          error("cannot iterate a heterogeneous tuple");
      }
    }

    LOG_TYPECHECK("[realize] fn {} -> {} : base {} ; depth = {}", type->name,
                  type->realizeString(), ctx->getBase(), depth);
    {
      ctx->addBlock();
      ctx->typecheckLevel++;
      ctx->bases.push_back({type->name, type, type->args[0]});
      auto clonedAst = ctx->cache->functions[type->name].ast->clone();
      auto *ast = (FunctionStmt *)clonedAst.get();
      addFunctionGenerics(type.get());

      // There is no AST linked to internal functions, so make sure not to parse it.
      bool isInternal = in(ast->attributes, ATTR_INTERNAL);
      isInternal |= ast->suite == nullptr;
      // Add function arguments.
      if (!isInternal)
        for (int i = 1; i < type->args.size(); i++) {
          seqassert(type->args[i] && type->args[i]->getUnbounds().empty(),
                    "unbound argument");
          ctx->add(TypecheckItem::Var, ast->args[i - 1].name,
                   make_shared<LinkType>(type->args[i]));
        }

      // Need to populate realization table in advance to make recursive functions work.
      ctx->cache->functions[type->name].realizations[type->realizeString()] = {type,
                                                                               nullptr};
      // Realizations are stored in the top-most base.
      ctx->bases[0].visitedAsts[type->realizeString()] = {TypecheckItem::Func, type};

      StmtPtr realized = nullptr;
      if (!isInternal) {
        auto inferred = inferTypes(move(ast->suite));
        LOG_TYPECHECK("realized {} in {} iterations", type->realizeString(),
                      inferred.first);
        realized = move(inferred.second);
        // Return type was not specified and the function returned nothing.
        if (!ast->ret && type->args[0]->getUnbound())
          type->args[0] |= ctx->findInternal("void");
      }
      // Realize the return type.
      realizeType(type->args[0]);
      // Create and store a realized AST to be used during the code generation.
      seqassert(ast->args.size() == type->args.size() - 1,
                "type/AST argument mismatch");
      vector<Param> args;
      for (auto &i : ast->args)
        args.emplace_back(Param{i.name, nullptr, nullptr});
      ctx->cache->functions[type->name].realizations[type->realizeString()].ast =
          Nx<FunctionStmt>(ast, type->realizeString(), nullptr, vector<Param>(),
                           move(args), move(realized),
                           map<string, string>(ast->attributes));
      ctx->bases.pop_back();
      ctx->popBlock();
      ctx->typecheckLevel--;
      LOG_REALIZE("done with {}", type->realizeString());
    }
    // Restore old bases back.
    ctx->bases.insert(ctx->bases.end(), oldBases.begin(), oldBases.end());
    return type;
  } catch (exc::ParserException &e) {
    e.trackRealize(fmt::format("{} (arguments {})", type->name, type->toString()),
                   getSrcInfo());
    throw;
  }
}

pair<int, StmtPtr> TypecheckVisitor::inferTypes(StmtPtr &&stmt, bool keepLast) {
  if (!stmt)
    return {0, nullptr};
  StmtPtr result = move(stmt);

  // We keep running typecheck transformations until there are no more unbound
  // types. It is assumed that the unbound count will decrease in each
  // iteration--- if not, the program cannot be type-checked.
  // TODO: this can be probably optimized one day...
  int minUnbound = ctx->cache->unboundCount;
  int iter = 0;
  for (int prevSize = INT_MAX;; iter++, ctx->iteration++) {
    ctx->addBlock();
    if (keepLast) // reset extendCount in whole code loop
      ctx->extendCount = 0;
    result = TypecheckVisitor(ctx).transform(result);

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
  return {iter, move(result)};
}
//
//
// pair<int, StmtPtr> TypecheckVisitor::inferTypes(StmtPtr &&stmt, bool keepLast) {
//  if (!stmt)
//    return {-1, nullptr};
//  StmtPtr result = move(stmt);
//
//  // Keep running typecheck inference until we are done (meaning that there are no
//  more
//  // unbound types and all transformations and variable substitutions are done). It
//  is
//  // assumed that the number of unbound types will decrease in each iteration. If
//  this
//  // does not happen, the types of a given program cannot be inferred.
//  ctx->typecheckLevel++;
//  auto oldIter = ctx->iteration;
//  ctx->iteration = 0;
//  int prevUnbound = ctx->cache->unboundCount;
//  int prevSize = INT_MAX;
//  while (ctx->iteration < TYPECHECK_MAX_ITERATIONS) {
//    ctx->addBlock();
//    if (keepLast) // reset extendCount in a whole code loop
//      ctx->extendCount = 0;
//    result = TypecheckVisitor(ctx).transform(result);
//
//    // Clean up unified unbound variables and check if there are new unbound
//    variables. int newUnboundSize = 0; set<types::TypePtr> newActiveUnbounds; for
//    (auto i = ctx->activeUnbounds.begin(); i != ctx->activeUnbounds.end();) {
//      auto l = (*i)->getLink();
//      assert(l);
//      if (l->kind == LinkType::Unbound) {
//        newActiveUnbounds.insert(*i);
//        if (l->id >= prevUnbound)
//          newUnboundSize++;
//      }
//      ++i;
//    }
//    ctx->activeUnbounds = newActiveUnbounds;
//    seqassert(!result->done || ctx->activeUnbounds.empty(),
//              "inconsistent stmt->done with unbound count");
//    if (result && result->done) {
//      if (!keepLast)
//        ctx->popBlock();
//      break;
//    }
//    if (newUnboundSize >= prevSize) {
//      // Number of unbound types either increased or stayed the same: we cannot
//      // typecheck this program!
//      vector<TypePtr> unresolved;
//      string errorMsg;
//      for (auto &ub : ctx->activeUnbounds)
//        if (ub->getUnbound()->id >= prevUnbound) {
//          unresolved.emplace_back(ub);
//          errorMsg +=
//              fmt::format("- {}:{}:{}: unresolved type\n", ub->getSrcInfo().file,
//                          ub->getSrcInfo().line, ub->getSrcInfo().col);
//          LOG_TYPECHECK("[inferTypes] dangling {} @ {}", ub->toString(),
//                        ub->getSrcInfo());
//        }
//      error(unresolved[0], "cannot resolve {} unbound variables:\n{}",
//            unresolved.size(), errorMsg);
//    }
//    prevSize = newUnboundSize;
//    LOG_TYPECHECK("=========================== {}",
//                  ctx->bases.back().type ? ctx->bases.back().type->toString() :
//                  "-");
//    ctx->iteration++;
//  }
//  std::swap(ctx->iteration, oldIter);
//  ctx->typecheckLevel--;
//  return make_pair(oldIter, move(result));
//}

} // namespace ast
} // namespace seq