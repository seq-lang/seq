/*
 * cache.cpp --- Code transformation cache (shared objects).
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "parser/cache.h"
#include "parser/common.h"
#include "parser/visitors/codegen/codegen.h"
#include "parser/visitors/typecheck/typecheck.h"
#include "parser/visitors/typecheck/typecheck_ctx.h"

namespace seq {
namespace ast {

Cache::Cache(string argv0)
    : generatedSrcInfoCount(0), unboundCount(0), varCount(0), age(0), testFlags(0),
      argv0(move(argv0)), module(nullptr) {}

string Cache::getTemporaryVar(const string &prefix, char sigil) {
  return fmt::format("{}{}_{}", sigil ? fmt::format("{}_", sigil) : "", prefix,
                     ++varCount);
}

SrcInfo Cache::generateSrcInfo() {
  return {FILE_GENERATED, generatedSrcInfoCount, generatedSrcInfoCount++, 0, 0};
}

types::ClassTypePtr Cache::findClass(const string &name) const {
  auto f = typeCtx->find(name);
  if (f->kind == TypecheckItem::Type)
    return f->type->getClass();
  return nullptr;
}

types::FuncTypePtr Cache::findFunction(const string &name) const {
  auto f = typeCtx->find(name);
  if (f->kind == TypecheckItem::Func)
    return f->type->getFunc();
  return nullptr;
}

types::FuncTypePtr Cache::findMethod(types::ClassType *typ, const string &member,
                                     const vector<pair<string, types::TypePtr>> &args) {
  return typeCtx->findBestMethod(typ, member, args);
}

ir::types::Type *Cache::realizeType(types::ClassTypePtr type,
                                    vector<types::TypePtr> generics) {
  type = typeCtx->instantiateGeneric(type->getSrcInfo(), type, generics)->getClass();
  auto tv = TypecheckVisitor(typeCtx);
  if (auto rtv = tv.realize(type)->getClass()) {
    return classes[rtv->name].realizations[rtv->realizedTypeName()].ir;
  }
  return nullptr;
}

ir::Func *Cache::realizeFunction(types::FuncTypePtr type, vector<types::TypePtr> args,
                                 vector<types::TypePtr> generics) {
  type = typeCtx->instantiate(type->getSrcInfo(), type)->getFunc();
  if (args.size() != type->args.size())
    return nullptr;
  for (int gi = 0; gi < args.size(); gi++) {
    types::Type::Unification undo;
    if (type->args[gi]->unify(args[gi].get(), &undo) < 0) {
      undo.undo();
      return nullptr;
    }
  }
  if (generics.size()) {
    if (generics.size() != type->funcGenerics.size())
      return nullptr;
    for (int gi = 0; gi < generics.size(); gi++) {
      types::Type::Unification undo;
      if (type->funcGenerics[gi].type->unify(generics[gi].get(), &undo) < 0) {
        undo.undo();
        return nullptr;
      }
    }
  }
  auto tv = TypecheckVisitor(typeCtx);
  if (auto rtv = tv.realize(type)->getFunc()) {
    auto &f = functions[rtv->funcName].realizations[rtv->realizedName()];
    auto *main = ir::cast<ir::BodiedFunc>(module->getMainFunc());
    auto *block = module->Nr<ir::SeriesFlow>("body");
    main->setBody(block);

    auto ctx = make_shared<CodegenContext>(shared_from_this(), block, main);
    auto toRealize = CodegenVisitor::initializeContext(ctx);
    for (auto &fnName : toRealize)
      CodegenVisitor(ctx).transform(functions[fnName].ast->clone());
    return f.ir;
  }
  return nullptr;
}

} // namespace ast
} // namespace seq