#include <libgen.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <utility>
#include <vector>

#include "parser/ast/codegen/codegen.h"
#include "parser/ast/codegen/codegen_ctx.h"
#include "parser/ast/context.h"
#include "parser/common.h"
#include "parser/ocaml.h"

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
  t = t->getClass();
  seqassert(t, "type must be set and a class");
  seqassert(t->canRealize(), "{} must be realizable", t->toString());
  auto it = types.find(t->realizeString());
  if (it != types.end())
    return it->second;

  seq::ir::types::Type *handle = nullptr;
  vector<seq::ir::types::Type *> types;
  vector<types::ClassTypePtr> unrealizedTypes;
  vector<int> statics;
  for (auto &m : t->explicits)
    if (auto s = m.type->getStatic())
      statics.push_back(s->getValue());
    else {
      types.push_back(realizeType(m.type->getClass()));
      unrealizedTypes.push_back(m.type->getClass());
    }
  auto name = t->name;
  if (name == ".void") {
    handle = new seq::ir::types::Type(name);
  } else if (name == ".bool") {
    handle = new seq::ir::types::Type(name);
  } else if (name == ".byte") {
    handle = new seq::ir::types::Type(name);
  } else if (name == ".int") {
    handle = new seq::ir::types::Type(name);
  } else if (name == ".float") {
    handle = new seq::ir::types::Type(name);
  } else if (name == ".str") {
    auto *bytePtrType = getPointer(make_shared<types::ClassType>(".byte"));
    auto *intType = realizeType(make_shared<types::ClassType>(".int"));
    handle = new seq::ir::types::RecordType(name, {intType, bytePtrType},
                                            {"len", "ptr"});
  } else if (name == ".Int" || name == ".UInt") {
    assert(statics.size() == 1 && types.size() == 0);
    assert(statics[0] >= 1 && statics[0] <= 2048);
    handle = new seq::ir::types::IntNType(statics[0], name == ".Int");
  } else if (name == ".Array") {
    assert(types.size() == 1 && statics.size() == 0);
    handle = new seq::ir::types::ArrayType(
        getPointer(unrealizedTypes[0]),
        realizeType(std::make_shared<types::ClassType>(".bool")));
  } else if (name == ".Ptr") {
    assert(types.size() == 1 && statics.size() == 0);
    handle = new seq::ir::types::PointerType(types[0]);
  } else if (name == ".Generator") {
    assert(types.size() == 1 && statics.size() == 0);
    handle = new seq::ir::types::GeneratorType(types[0]);
  } else if (name == ".Optional") {
    assert(types.size() == 1 && statics.size() == 0);
    handle = new seq::ir::types::OptionalType(
        getPointer(unrealizedTypes[0]),
        realizeType(std::make_shared<types::ClassType>(".bool")));
  } else if (startswith(name, ".Function.")) {
    types.clear();
    for (auto &m : t->args)
      types.push_back(realizeType(m->getClass()));
    auto ret = types[0];
    types.erase(types.begin());
    handle = new seq::ir::types::FuncType(name, ret, types);
  } else {
    vector<string> names;
    vector<seq::ir::types::Type *> types;
    seq::ir::types::RecordType *record;
    if (t->isRecord()) {
      handle = record = new seq::ir::types::RecordType(name);
    } else {
      auto contents = std::make_unique<seq::ir::types::RecordType>(name + ".contents");
      record = contents.get();
      handle = new seq::ir::types::RefType(name, std::move(contents));
    }
    this->types[t->realizeString()] = handle;

    // Must do this afterwards to avoid infinite loop with recursive types
    for (auto &m : cache->memberRealizations[t->realizeString()]) {
      names.push_back(m.first);
      types.push_back(realizeType(m.second->getClass()));
    }
    record->realize(types, names);
  }
  getModule()->types.push_back(std::unique_ptr<seq::ir::types::Type>(handle));
  return this->types[t->realizeString()] = handle;
}

seq::ir::types::ArrayType *CodegenContext::getArgvType() {
  auto *strPointerType = getPointer(make_shared<types::ClassType>(".str"));
  auto *intType = realizeType(make_shared<types::ClassType>(".int"));
  auto arrayName = ".Array[.str]";

  auto it = types.find(arrayName);
  if (it != types.end())
    return dynamic_cast<seq::ir::types::ArrayType *>(it->second);

  auto array = std::make_unique<seq::ir::types::ArrayType>(strPointerType, intType);
  this->types[arrayName] = array.get();
  getModule()->types.push_back(std::move(array));
  return (seq::ir::types::ArrayType *)this->types[arrayName];

}

seq::ir::types::PointerType *CodegenContext::getPointer(types::ClassTypePtr t) {
  t = t->getClass();
  auto pointerName = fmt::format(FMT_STRING(".Ptr[{}]"), t->realizeString());
  auto it = types.find(pointerName);
  if (it != types.end())
    return dynamic_cast<seq::ir::types::PointerType *>(it->second);

  auto pointer = std::make_unique<seq::ir::types::PointerType>(realizeType(t));
  this->types[pointerName] = pointer.get();
  getModule()->types.push_back(std::move(pointer));
  return (seq::ir::types::PointerType *)this->types[pointerName];
}

} // namespace ast
} // namespace seq
