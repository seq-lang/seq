#include <libgen.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <utility>
#include <vector>

#include "parser/common.h"
#include "parser/ctx.h"
#include "parser/ocaml/ocaml.h"
#include "parser/visitors/codegen/codegen.h"
#include "parser/visitors/codegen/codegen_ctx.h"

using fmt::format;
using std::make_pair;

namespace seq {
namespace ast {

CodegenContext::CodegenContext(shared_ptr<Cache> cache, seq::ir::SeriesFlow *top,
                               seq::ir::Func *base)
    : Context<CodegenItem>(""), cache(std::move(cache)) {
  stack.push_front(vector<string>());
  topBaseIndex = topBlockIndex = 0;
  if (top)
    addSeries(top, base);
}

shared_ptr<CodegenItem> CodegenContext::find(const string &name, bool onlyLocal,
                                             bool checkStdlib) const {
  auto i = Context<CodegenItem>::find(name);
  if (i)
    return i;
  return nullptr;
}

void CodegenContext::addVar(const string &name, seq::ir::Var *v, bool global) {
  auto i =
      make_shared<CodegenItem>(CodegenItem::Var, getBase(), global || isToplevel());
  i->handle.var = v;
  add(name, i);
}

void CodegenContext::addType(const string &name, seq::ir::types::Type *t, bool global) {
  auto i =
      make_shared<CodegenItem>(CodegenItem::Type, getBase(), global || isToplevel());
  i->handle.type = t;
  add(name, i);
}

void CodegenContext::addFunc(const string &name, seq::ir::Func *f, bool global) {
  auto i =
      make_shared<CodegenItem>(CodegenItem::Func, getBase(), global || isToplevel());
  i->handle.func = f;
  add(name, i);
}

void CodegenContext::addSeries(seq::ir::SeriesFlow *s, seq::ir::Func *newBase) {
  if (s)
    topBlockIndex = series.size();
  series.push_back(s);
  if (newBase)
    topBaseIndex = bases.size();
  bases.push_back(newBase);
}

void CodegenContext::popSeries() {
  bases.pop_back();
  topBaseIndex = bases.size() - 1;
  while (topBaseIndex && !bases[topBaseIndex])
    topBaseIndex--;
  series.pop_back();
  topBlockIndex = series.size() - 1;
  while (topBlockIndex && !series[topBlockIndex])
    topBlockIndex--;
}

// void CodegenContext::initJIT() {
//  jit = new seq::SeqJIT();
//  auto fn = new seq::Func();
//  fn->setName(".jit_0");
//
//  addBlock(fn->getBlock(), fn);
//  assert(topBaseIndex == topBlockIndex && topBlockIndex == 0);
//
//  execJIT();
//}
//
// void CodegenContext::execJIT(string varName, seq::Expr *varExpr) {
//  // static int counter = 0;
//
//  // assert(jit != nullptr);
//  // assert(bases.size() == 1);
//  // jit->addFunc((seq::Func *)bases[0]);
//
//  // vector<pair<string, shared_ptr<CodegenItem>>> items;
//  // for (auto &name : stack.front()) {
//  //   auto i = find(name);
//  //   if (i && i->isGlobal())
//  //     items.push_back(make_pair(name, i));
//  // }
//  // popBlock();
//  // for (auto &i : items)
//  //   add(i.first, i.second);
//  // if (varExpr) {
//  //   auto var = jit->addVar(varExpr);
//  //   add(varName, var);
//  // }
//
//  // // Set up new block
//  // auto fn = new seq::Func();
//  // fn->setName(format(".jit_{}", ++counter));
//  // addBlock(fn->getBlock(), fn);
//  // assert(topBaseIndex == topBlockIndex && topBlockIndex == 0);
//}

seq::ir::types::Type *CodegenContext::realizeType(types::ClassTypePtr t) {
  //  t = t->getClass();
  seqassert(t, "type must be set and a class");
  seqassert(t->canRealize(), "{} must be realizable", t->toString());
  auto it = types.find(t->realizeString());
  if (it != types.end())
    return it->second;

  seq::ir::types::Type *handle = nullptr;
  vector<seq::ir::types::Type *> types;
  vector<int> statics;
  for (auto &m : t->explicits) {
    if (auto s = m.type->getStatic())
      statics.push_back(s->getValue());
    else
      types.push_back(realizeType(m.type->getClass()));
  }

  auto name = t->name;
  auto *module = getModule();

  if (name == ".void") {
    handle = module->getVoidType();
  } else if (name == ".bool") {
    handle = module->getBoolType();
  } else if (name == ".byte") {
    handle = module->getByteType();
  } else if (name == ".int") {
    handle = module->getIntType();
  } else if (name == ".float") {
    handle = module->getFloatType();
  } else if (name == ".str") {
    handle = module->getStringType();
  } else if (name == ".Int" || name == ".UInt") {
    assert(statics.size() == 1 && types.size() == 0);
    assert(statics[0] >= 1 && statics[0] <= 2048);
    handle = module->getIntNType(statics[0], name == ".Int");
  } else if (name == ".Array") {
    assert(types.size() == 1 && statics.size() == 0);
    handle = module->getArrayType(types[0]);
  } else if (name == ".Ptr") {
    assert(types.size() == 1 && statics.size() == 0);
    handle = module->getPointerType(types[0]);
  } else if (name == ".Generator") {
    assert(types.size() == 1 && statics.size() == 0);
    handle = module->getGeneratorType(types[0]);
  } else if (name == ".Optional") {
    assert(types.size() == 1 && statics.size() == 0);
    handle = module->getOptionalType(types[0]);
  } else if (startswith(name, ".Function.")) {
    types.clear();
    for (auto &m : t->args)
      types.push_back(realizeType(m->getClass()));
    auto ret = types[0];
    types.erase(types.begin());
    handle = module->getFuncType(ret, types);
  } else {
    vector<string> names;
    vector<seq::ir::types::Type *> types;

    handle = module->getMemberedType(t->realizeString(), !t->isRecord());
    this->types[t->realizeString()] = handle;

    // Must do this afterwards to avoid infinite loop with recursive types
    for (auto &m : cache->classes[t->name].realizations[t->realizeString()].fields) {
      names.push_back(m.first);
      types.push_back(realizeType(m.second->getClass()));
    }

    dynamic_cast<ir::types::MemberedType *>(handle)->realize(types, names);
  }
  return this->types[t->realizeString()] = handle;
}

} // namespace ast
} // namespace seq
