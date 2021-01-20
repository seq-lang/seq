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

#include "sir/types/types.h"

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
  if (!typ || !typ->getClass() || !typ->canRealize())
    return nullptr;
  auto type = typ->getClass();
  // We are still not done with type creation...
  for (auto &m : ctx->cache->classes[type->name].fields)
    if (!m.type)
      return nullptr;

  try {
    ClassTypePtr realizedType = nullptr;
    auto it = ctx->cache->classes[type->name].realizations.find(type->realizeString());
    if (it != ctx->cache->classes[type->name].realizations.end()) {
      realizedType = it->second.type->getClass();
    } else {
      realizedType = type->getClass();
      // Realize generics
      for (auto &e : realizedType->generics)
        if (!e.type->getStatic())
          if (!realizeType(e.type))
            return nullptr;

      LOG_TYPECHECK("[realize] ty {} -> {}", type->name, type->realizeString());
      // Realizations are stored in the top-most base.
      ctx->bases[0].visitedAsts[realizedType->realizeString()] = {TypecheckItem::Type,
                                                                  realizedType};
      ctx->cache->classes[realizedType->name]
          .realizations[realizedType->realizeString()] = {realizedType, {}, nullptr};
      // Realize arguments
      for (auto &a : realizedType->args)
        realizeType(a);
      auto lt = getLLVMType(realizedType.get());
      // Realize fields.
      vector<const seq::ir::types::Type *> typeArgs;
      vector<string> names;
      map<std::string, SrcInfo> memberInfo;
      for (auto &m : ctx->cache->classes[realizedType->name].fields) {
        auto mt =
            ctx->instantiate(realizedType->getSrcInfo(), m.type, realizedType.get());
        LOG_REALIZE("- member: {} -> {}: {}", m.name, m.type->toString(),
                    mt->toString());
        auto tf = realizeType(mt);
        seqassert(tf, "cannot realize {}.{}", typ->realizeString(), m.name);
        ctx->cache->classes[realizedType->name]
            .realizations[realizedType->realizeString()]
            .fields.emplace_back(m.name, tf);
        names.emplace_back(m.name);
        typeArgs.emplace_back(getLLVMType(tf->getClass().get()));
        memberInfo[m.name] = mt->getSrcInfo();
      }
      if (auto *cls = seq::ir::cast<seq::ir::types::RefType>(lt))
        if (!names.empty()) {
          cls->getContents()->realize(typeArgs, names);
          cls->setAttribute(
              std::make_unique<seq::ir::MemberAttribute>(std::move(memberInfo)));
          cls->getContents()->setAttribute(
              std::make_unique<seq::ir::MemberAttribute>(std::move(memberInfo)));
        }
    }
    return realizedType;
  } catch (exc::ParserException &e) {
    e.trackRealize(type->toString(), getSrcInfo());
    throw;
  }
}

types::TypePtr TypecheckVisitor::realizeFunc(const types::TypePtr &typ) {
  if (!typ || !typ->getFunc() || !typ->canRealize())
    return nullptr;
  auto type = typ->getFunc();

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
        break;
      }
    }
    auto oldBases = vector<TypeContext::RealizationBase>(ctx->bases.begin() + depth,
                                                         ctx->bases.end());
    while (ctx->bases.size() > depth)
      ctx->bases.pop_back();
    if (ctx->realizationDepth > 500)
      seq::compilationError(
          "maximum realization depth exceeded (recursive static function?)",
          getSrcInfo().file, getSrcInfo().line, getSrcInfo().col);

    // Special cases: Tuple.(__iter__, __getitem__, __contains__).
    if (endswith(type->name, ".__iter__")) {
      // __iter__ in an empty tuple.
      auto s = ctx->cache->functions[type->name].ast->suite->getSuite();
      if (s && s->stmts.empty())
        error("cannot iterate empty tuple");
    }
    if (startswith(type->name, "Tuple.N") &&
        (endswith(type->name, ".__iter__") || endswith(type->name, ".__getitem__") ||
         endswith(type->name, ".__contains__"))) {
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
      _level++;
      ctx->realizationDepth++;
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

      // Need to populate realization table in advance to make recursive functions
      // work.
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
      type->args[0] |= realizeType(type->args[0]);
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
      ctx->realizationDepth--;
      LOG_REALIZE("done with {}", type->realizeString());
      _level--;
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
  ctx->addBlock();
  for (int prevSize = INT_MAX;; iter++, ctx->iteration++) {
    LOG_TYPECHECK("== iter {} ==========================================", iter);
    ctx->typecheckLevel++;
    result = TypecheckVisitor(ctx).transform(result);
    ctx->typecheckLevel--;

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

    //    ctx->popBlock();
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
  }
  // Last pass; TODO: detect if it is needed...
  //  ctx->addBlock();
  LOG_TYPECHECK("== iter {} ==========================================", ++iter);
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

seq::ir::types::Type *TypecheckVisitor::getLLVMType(const types::ClassType *t) {
  if (auto l = ctx->cache->classes[t->name].realizations[t->realizeString()].llvm)
    return l;
  auto getLLVM = [&](const TypePtr &tt) {
    auto t = tt->getClass();
    seqassert(t && in(ctx->cache->classes[t->name].realizations, t->realizeString()),
              "{} not realized", tt->toString());
    auto l = ctx->cache->classes[t->name].realizations[t->realizeString()].llvm;
    seqassert(l, "no LLVM type for {}", t->toString());
    return l;
  };

  seq::ir::types::Type *handle = nullptr;
  vector<const seq::ir::types::Type *> types;
  vector<int> statics;
  for (auto &m : t->generics)
    if (auto s = m.type->getStatic()) {
      seqassert(s->staticEvaluation.first, "static not realized");
      statics.push_back(s->staticEvaluation.second);
    } else {
      types.push_back(getLLVM(m.type));
    }
  auto name = t->name;
  if (name == "void") {
    handle = ctx->cache->module->getVoidType();
  } else if (name == "bool") {
    handle = ctx->cache->module->getBoolType();
  } else if (name == "byte") {
    handle = ctx->cache->module->getByteType();
  } else if (name == "int") {
    handle = ctx->cache->module->getIntType();
  } else if (name == "float") {
    handle = ctx->cache->module->getFloatType();
  } else if (name == "str") {
    handle = ctx->cache->module->getStringType();
  } else if (name == "Int" || name == "UInt") {
    assert(statics.size() == 1 && types.empty());
    handle = ctx->cache->module->getIntNType(statics[0], name == "Int");
  } else if (name == "Ptr") {
    assert(types.size() == 1 && statics.empty());
    handle = ctx->cache->module->getPointerType(types[0]);
  } else if (name == "Generator") {
    assert(types.size() == 1 && statics.empty());
    handle = ctx->cache->module->getGeneratorType(types[0]);
  } else if (name == "Optional") {
    assert(types.size() == 1 && statics.empty());
    handle = ctx->cache->module->getOptionalType(types[0]);
  } else if (startswith(name, "Function.N")) {
    types.clear();
    for (auto &m : t->args)
      types.push_back(getLLVM(m));
    auto ret = types[0];
    types.erase(types.begin());
    handle = ctx->cache->module->getFuncType(ret, types);
  } else if (t->isRecord()) {
    vector<const seq::ir::types::Type *> typeArgs;
    vector<string> names;
    map<std::string, SrcInfo> memberInfo;
    for (int ai = 0; ai < t->args.size(); ai++) {
      names.emplace_back(ctx->cache->classes[t->name].fields[ai].name);
      typeArgs.emplace_back(getLLVM(t->args[ai]));
      memberInfo[ctx->cache->classes[t->name].fields[ai].name] =
          t->args[ai]->getSrcInfo();
    }
    auto record = seq::ir::cast<seq::ir::types::RecordType>(
        ctx->cache->module->getMemberedType(t->realizeString()));
    record->realize(typeArgs, names);
    handle = record;
    handle->setAttribute(
        std::make_unique<seq::ir::MemberAttribute>(std::move(memberInfo)));
  } else {
    // Type arguments will be populated afterwards to avoid infinite loop with recursive
    // reference types.
    handle = ctx->cache->module->getMemberedType(t->realizeString(), true);
  }
  handle->setSrcInfo(t->getSrcInfo());
  return ctx->cache->classes[t->name].realizations[t->realizeString()].llvm = handle;
}

} // namespace ast
} // namespace seq
