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

types::TypePtr TypecheckVisitor::realize(types::TypePtr typ) {
  if (!typ || !typ->getClass() || !typ->canRealize()) {
    return nullptr;
  } else if (auto f = typ->getFunc()) {
    auto ret = realizeFunc(f.get());
    if (ret)
      realizeType(ret->getClass().get());
    return ret;
  } else {
    auto t = realizeType(typ->getClass().get());
    if (auto p = typ->getPartial())
      return make_shared<PartialType>(t->getRecord(), p->func, p->known);
    else
      return t;
  }
}

types::TypePtr TypecheckVisitor::realizeType(types::ClassType *type) {
  seqassert(type->canRealize(), "{} not realizable", type->toString());

  // We are still not done with type creation...
  for (auto &m : ctx->cache->classes[type->name].fields)
    if (!m.type)
      return nullptr;

  if (startswith(type->name, TYPE_CALLABLE))
    error("realizing trait");

  auto realizedName = type->realizedTypeName();
  try {
    ClassTypePtr realizedType = nullptr;
    auto it = ctx->cache->classes[type->name].realizations.find(realizedName);
    if (it != ctx->cache->classes[type->name].realizations.end()) {
      realizedType = it->second->type->getClass();
    } else {
      realizedType = type->getClass();
      // Realize generics
      for (auto &e : realizedType->generics)
        if (!e.type->getStatic())
          if (!realize(e.type))
            return nullptr;
      realizedName = realizedType->realizedTypeName();

      LOG_TYPECHECK("[realize] ty {} -> {}", type->name, realizedName);
      // Realizations are stored in the top-most base.
      ctx->bases[0].visitedAsts[realizedName] = {TypecheckItem::Type, realizedType};
      auto r = ctx->cache->classes[realizedType->name].realizations[realizedName] =
          make_shared<Cache::Class::ClassRealization>();
      r->type = realizedType;
      // Realize arguments
      if (auto tr = realizedType->getRecord())
        for (auto &a : tr->args)
          realize(a);
      auto lt = getLLVMType(realizedType.get());
      // Realize fields.
      vector<ir::types::Type *> typeArgs;
      vector<string> names;
      map<std::string, SrcInfo> memberInfo;
      for (auto &m : ctx->cache->classes[realizedType->name].fields) {
        auto mt = ctx->instantiate(N<IdExpr>(m.name).get(), m.type, realizedType.get());
        LOG_REALIZE("- member: {} -> {}: {}", m.name, m.type->toString(),
                    mt->toString());
        auto tf = realize(mt);
        if (!tf)
          error("cannot realize {}.{} of type {}",
                ctx->cache->reverseIdentifierLookup[realizedType->name], m.name,
                mt->toString());
        // seqassert(tf, "cannot realize {}.{}: {}", realizedName, m.name,
        // mt->toString());
        r->fields.emplace_back(m.name, tf);
        names.emplace_back(m.name);
        typeArgs.emplace_back(getLLVMType(tf->getClass().get()));
        memberInfo[m.name] = m.type->getSrcInfo();
      }
      if (auto *cls = ir::cast<ir::types::RefType>(lt))
        if (!names.empty()) {
          cls->getContents()->realize(typeArgs, names);
          cls->setAttribute(std::make_unique<ir::MemberAttribute>(memberInfo));
          cls->getContents()->setAttribute(
              std::make_unique<ir::MemberAttribute>(memberInfo));
        }
    }
    return realizedType;
  } catch (exc::ParserException &e) {
    e.trackRealize(type->toString(), getSrcInfo());
    throw;
  }
}

types::TypePtr TypecheckVisitor::realizeFunc(types::FuncType *type) {
  seqassert(type->canRealize(), "{} not realizable", type->toString());

  try {
    auto it =
        ctx->cache->functions[type->funcName].realizations.find(type->realizedName());
    if (it != ctx->cache->functions[type->funcName].realizations.end())
      return it->second->type;

    // Set up bases. Ensure that we have proper parent bases even during a realization
    // of mutually recursive functions.
    int depth = 1;
    for (auto p = type->funcParent; p;) {
      if (auto f = p->getFunc()) {
        depth++;
        p = f->funcParent;
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
    if (startswith(type->funcName, TYPE_TUPLE) &&
        (endswith(type->funcName, ".__iter__") ||
         endswith(type->funcName, ".__getitem__")) &&
        type->args[1]->getHeterogenousTuple())
      error("cannot iterate a heterogeneous tuple");

    LOG_REALIZE("[realize] fn {} -> {} : base {} ; depth = {}", type->funcName,
                type->realizedName(), ctx->getBase(), depth);
    {
      _level++;
      ctx->realizationDepth++;
      ctx->addBlock();
      ctx->typecheckLevel++;
      ctx->bases.push_back({type->funcName, type->getFunc(), type->args[0]});
      auto clonedAst = ctx->cache->functions[type->funcName].ast->clone();
      auto *ast = (FunctionStmt *)clonedAst.get();
      addFunctionGenerics(type);

      // There is no AST linked to internal functions, so make sure not to parse it.
      bool isInternal = ast->attributes.has(Attr::Internal);
      isInternal |= ast->suite == nullptr;
      // Add function arguments.
      if (!isInternal)
        for (int i = 1; i < type->args.size(); i++) {
          seqassert(type->args[i] && type->args[i]->getUnbounds().empty(),
                    "unbound argument {}", type->args[i]->toString());

          string varName = ast->args[i - 1].name;
          trimStars(varName);
          ctx->add(
              TypecheckItem::Var, varName,
              make_shared<LinkType>(type->args[i]->generalize(ctx->typecheckLevel)));
        }

      // Need to populate realization table in advance to make recursive functions
      // work.
      auto oldKey = type->realizedName();
      auto r =
          ctx->cache->functions[type->funcName].realizations[type->realizedName()] =
              make_shared<Cache::Function::FunctionRealization>();
      r->type = type->getFunc();
      // Realizations are stored in the top-most base.
      ctx->bases[0].visitedAsts[type->realizedName()] = {TypecheckItem::Func,
                                                         type->getFunc()};

      StmtPtr realized = nullptr;
      if (!isInternal) {
        auto inferred = inferTypes(move(ast->suite));
        LOG_TYPECHECK("realized {} in {} iterations", type->realizedName(),
                      inferred.first);
        realized = move(inferred.second);
        // Return type was not specified and the function returned nothing.
        if (!ast->ret && type->args[0]->getUnbound())
          unify(type->args[0], ctx->findInternal("void"));
      }
      // Realize the return type.
      if (auto t = realize(type->args[0]))
        unify(type->args[0], t);
      LOG_TYPECHECK("done with {} / {}", type->realizedName(), oldKey);

      // Create and store IR node and a realized AST to be used
      // during the code generation.
      if (!in(ctx->cache->pendingRealizations,
              make_pair(type->funcName, type->realizedName()))) {
        if (ast->attributes.has(Attr::Internal)) {
          // This is either __new__, Ptr.__new__, etc.
          r->ir = ctx->cache->module->Nr<ir::InternalFunc>(type->funcName);
        } else if (ast->attributes.has(Attr::LLVM)) {
          r->ir = ctx->cache->module->Nr<ir::LLVMFunc>(type->realizedName());
        } else if (ast->attributes.has(Attr::C)) {
          r->ir = ctx->cache->module->Nr<ir::ExternalFunc>(type->realizedName());
        } else {
          r->ir = ctx->cache->module->Nr<ir::BodiedFunc>(type->realizedName());
          if (ast->attributes.has(Attr::ForceRealize))
            ir::cast<ir::BodiedFunc>(r->ir)->setBuiltin();
        }

        auto parent = type->funcParent;
        if (!ast->attributes.parentClass.empty() &&
            !ast->attributes.has(Attr::Method)) // hack for non-generic types
          parent = ctx->find(ast->attributes.parentClass)->type;
        if (parent && parent->canRealize()) {
          parent = realize(parent);
          r->ir->setParentType(getLLVMType(parent->getClass().get()));
        }
        r->ir->setGlobal();
        ctx->cache->pendingRealizations.insert({type->funcName, type->realizedName()});

        seqassert(!type || ast->args.size() == type->args.size() - 1,
                  "type/AST argument mismatch");
        vector<Param> args;
        for (auto &i : ast->args) {
          string varName = i.name;
          trimStars(varName);
          args.emplace_back(Param{varName, nullptr, nullptr});
        }
        r->ast = Nx<FunctionStmt>(ast, type->realizedName(), nullptr, vector<Param>(),
                                  move(args), move(realized), ast->attributes);
        ctx->cache->functions[type->funcName].realizations[type->realizedName()] = r;
      } else {
        ctx->cache->functions[type->funcName].realizations[oldKey] =
            ctx->cache->functions[type->funcName].realizations[type->realizedName()];
      }
      ctx->bases[0].visitedAsts[type->realizedName()] = {TypecheckItem::Func,
                                                         type->getFunc()};
      ctx->bases.pop_back();
      ctx->popBlock();
      ctx->typecheckLevel--;
      ctx->realizationDepth--;
      _level--;
    }
    // Restore old bases back.
    ctx->bases.insert(ctx->bases.end(), oldBases.begin(), oldBases.end());
    return type->getFunc();
  } catch (exc::ParserException &e) {
    e.trackRealize(fmt::format("{} (arguments {})", type->funcName, type->toString()),
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
    std::map<types::TypePtr, string> newActiveUnbounds;
    for (auto i = ctx->activeUnbounds.begin(); i != ctx->activeUnbounds.end();) {
      auto l = i->first->getLink();
      assert(l);
      if (l->kind == LinkType::Unbound) {
        newActiveUnbounds[i->first] = i->second;
        if (l->id >= minUnbound)
          newUnbounds++;
      }
      ++i;
    }
    ctx->activeUnbounds = newActiveUnbounds;

    if (ctx->activeUnbounds.empty() || !newUnbounds) {
      break;
    } else {
      if (newUnbounds >= prevSize) {
        map<int, pair<seq::SrcInfo, string>> v;
        for (auto &ub : ctx->activeUnbounds)
          if (ub.first->getLink()->id >= minUnbound) {
            v[ub.first->getLink()->id] = {ub.first->getSrcInfo(), ub.second};
            LOG_TYPECHECK("dangling ?{} ({})", ub.first->getLink()->id, minUnbound);
          }
        for (auto &ub : v) {
          seq::compilationError(format("cannot infer the type of {}", ub.second.second),
                                ub.second.first.file, ub.second.first.line,
                                ub.second.first.col, /*terminate=*/false);
        }
        error("cannot typecheck the program");
      }
      prevSize = newUnbounds;
    }
  }
  // Last pass; TODO: detect if it is needed...
  //  ctx->addBlock();
  LOG_TYPECHECK("== iter {} ==========================================", ++iter);
  for (int i = 0; i < 1; i++, iter++) {
    ctx->typecheckLevel++;
    result = TypecheckVisitor(ctx).transform(result);
    ctx->typecheckLevel--;
  }
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

ir::types::Type *TypecheckVisitor::getLLVMType(const types::ClassType *t) {
  auto realizedName = t->realizedTypeName();
  if (auto l = ctx->cache->classes[t->name].realizations[realizedName]->ir)
    return l;
  auto getLLVM = [&](const TypePtr &tt) {
    auto t = tt->getClass();
    seqassert(t && in(ctx->cache->classes[t->name].realizations, t->realizedTypeName()),
              "{} not realized", tt->toString());
    auto l = ctx->cache->classes[t->name].realizations[t->realizedTypeName()]->ir;
    seqassert(l, "no LLVM type for {}", t->toString());
    return l;
  };

  ir::types::Type *handle = nullptr;
  vector<ir::types::Type *> types;
  vector<int> statics;
  for (auto &m : t->generics)
    if (auto s = m.type->getStatic()) {
      seqassert(s->staticEvaluation.first, "static not realized");
      statics.push_back(s->staticEvaluation.second);
    } else {
      types.push_back(getLLVM(m.type));
    }
  auto name = t->name;
  auto *module = ctx->cache->module;

  if (name == "void") {
    handle = module->getVoidType();
  } else if (name == "bool") {
    handle = module->getBoolType();
  } else if (name == "byte") {
    handle = module->getByteType();
  } else if (name == "int") {
    handle = module->getIntType();
  } else if (name == "float") {
    handle = module->getFloatType();
  } else if (name == "str") {
    handle = module->getStringType();
  } else if (name == "Int" || name == "UInt") {
    assert(statics.size() == 1 && types.empty());
    handle = module->Nr<ir::types::IntNType>(statics[0], name == "Int");
  } else if (name == "Ptr") {
    assert(types.size() == 1 && statics.empty());
    handle = module->unsafeGetPointerType(types[0]);
  } else if (name == "Generator") {
    assert(types.size() == 1 && statics.empty());
    handle = module->unsafeGetGeneratorType(types[0]);
  } else if (name == TYPE_OPTIONAL) {
    assert(types.size() == 1 && statics.empty());
    handle = module->unsafeGetOptionalType(types[0]);
  } else if (startswith(name, TYPE_FUNCTION)) {
    types.clear();
    for (auto &m : const_cast<ClassType *>(t)->getRecord()->args)
      types.push_back(getLLVM(m));
    auto ret = types[0];
    types.erase(types.begin());
    handle = module->unsafeGetFuncType(realizedName, ret, types);
  } else if (auto tr = const_cast<ClassType *>(t)->getRecord()) {
    vector<ir::types::Type *> typeArgs;
    vector<string> names;
    map<std::string, SrcInfo> memberInfo;
    for (int ai = 0; ai < tr->args.size(); ai++) {
      names.emplace_back(ctx->cache->classes[t->name].fields[ai].name);
      typeArgs.emplace_back(getLLVM(tr->args[ai]));
      memberInfo[ctx->cache->classes[t->name].fields[ai].name] =
          ctx->cache->classes[t->name].fields[ai].type->getSrcInfo();
    }
    auto record =
        ir::cast<ir::types::RecordType>(module->unsafeGetMemberedType(realizedName));
    record->realize(typeArgs, names);
    handle = record;
    handle->setAttribute(std::make_unique<ir::MemberAttribute>(std::move(memberInfo)));
  } else {
    // Type arguments will be populated afterwards to avoid infinite loop with recursive
    // reference types.
    handle = module->unsafeGetMemberedType(realizedName, true);
  }
  handle->setSrcInfo(t->getSrcInfo());
  handle->setAstType(
      std::const_pointer_cast<seq::ast::types::Type>(t->shared_from_this()));
  // Not needed for classes, I guess
  //  if (auto &ast = ctx->cache->classes[t->name].ast)
  //    handle->setAttribute(std::make_unique<ir::KeyValueAttribute>(ast->attributes));
  return ctx->cache->classes[t->name].realizations[realizedName]->ir = handle;
}

} // namespace ast
} // namespace seq
