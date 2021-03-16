/*
 * codegen_ctx.cpp --- Context for IR translation stage.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */
#include <memory>
#include <string>
#include <vector>

#include "codegen.h"
#include "codegen_ctx.h"
#include "parser/common.h"
#include "parser/ctx.h"
#include "parser/ocaml/ocaml.h"
#include "parser/visitors/codegen/codegen.h"
#include "parser/visitors/codegen/codegen_ctx.h"
#include "parser/visitors/typecheck/typecheck_ctx.h"

namespace seq {
namespace ast {

CodegenContext::CodegenContext(shared_ptr<Cache> cache, seq::ir::SeriesFlow *series,
                               seq::ir::BodiedFunc *base)
    : Context<CodegenItem>(""), cache(std::move(cache)) {
  stack.push_front(vector<string>());
  bases.push_back(base);
  addSeries(series);
}

shared_ptr<CodegenItem> CodegenContext::find(const string &name) const {
  if (auto t = Context<CodegenItem>::find(name))
    return t;
  shared_ptr<CodegenItem> ret = nullptr;
  auto tt = cache->typeCtx->find(name);
  if (tt->isType() && tt->type->canRealize()) {
    ret = make_shared<CodegenItem>(CodegenItem::Type, bases[0]);
    seqassert(in(cache->classes, tt->type->getClass()->name) &&
                  in(cache->classes[tt->type->getClass()->name].realizations, name),
              "cannot find type realization {}", name);
    ret->handle.type =
        cache->classes[tt->type->getClass()->name].realizations[name]->ir;
  } else if (tt->type->getFunc() && tt->type->canRealize()) {
    ret = make_shared<CodegenItem>(CodegenItem::Func, bases[0]);
    seqassert(
        in(cache->functions, tt->type->getFunc()->funcName) &&
            in(cache->functions[tt->type->getFunc()->funcName].realizations, name),
        "cannot find type realization {}", name);
    ret->handle.func =
        cache->functions[tt->type->getFunc()->funcName].realizations[name]->ir;
  }
  return ret;
}

shared_ptr<CodegenItem> CodegenContext::add(CodegenItem::Kind kind, const string &name,
                                            void *type) {
  auto it = make_shared<CodegenItem>(kind, getBase());
  if (kind == CodegenItem::Var)
    it->handle.var = (ir::Var *)type;
  else if (kind == CodegenItem::Func)
    it->handle.func = (ir::Func *)type;
  else
    it->handle.type = (ir::types::Type *)type;
  add(name, it);
  return it;
}

void CodegenContext::addSeries(seq::ir::SeriesFlow *s) { series.push_back(s); }
void CodegenContext::popSeries() { series.pop_back(); }

seq::ir::Module *CodegenContext::getModule() const {
  return dynamic_cast<seq::ir::Module *>(bases[0]->getModule());
}
seq::ir::BodiedFunc *CodegenContext::getBase() const { return bases.back(); }
seq::ir::SeriesFlow *CodegenContext::getSeries() const { return series.back(); }

} // namespace ast
} // namespace seq
