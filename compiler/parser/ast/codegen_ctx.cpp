#include <libgen.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/codegen.h"
#include "parser/ast/codegen_ctx.h"
#include "parser/ast/context.h"
#include "parser/ast/format.h"
#include "parser/ast/transform.h"
#include "parser/ast/transform_ctx.h"
#include "parser/common.h"
#include "parser/ocaml.h"

using fmt::format;
using std::make_pair;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;

namespace seq {
namespace ast {

LLVMContext::LLVMContext(const string &filename,
                         shared_ptr<RealizationContext> realizations,
                         shared_ptr<ImportContext> imports, seq::Block *block,
                         seq::BaseFunc *base, seq::SeqJIT *jit)
    : Context<LLVMItem::Item>(filename, realizations, imports),
      tryCatch(nullptr), jit(jit) {
  stack.push_front(vector<string>());
  topBaseIndex = topBlockIndex = 0;
  if (block)
    addBlock(block, base);
}

LLVMContext::~LLVMContext() {}

shared_ptr<LLVMItem::Item> LLVMContext::find(const string &name, bool onlyLocal,
                                             bool checkStdlib) const {
  auto i = Context<LLVMItem::Item>::find(name);
  if (i && CAST(i, LLVMItem::Var)) {
    if (onlyLocal)
      return (getBase() == i->getBase()) ? i : nullptr;
    else
      return i;
  } else if (i) {
    return i;
  } else {
    auto stdlib = imports->getImport("");
    if (stdlib && checkStdlib)
      return stdlib->lctx->find(name, onlyLocal, false);
    else
      return nullptr;
  }
}

void LLVMContext::addVar(const string &name, seq::Var *v, bool global) {
  add(name, make_shared<LLVMItem::Var>(v, getBase(), global || isToplevel()));
}

void LLVMContext::addType(const string &name, seq::types::Type *t,
                          bool global) {
  add(name, make_shared<LLVMItem::Class>(t, getBase(), global || isToplevel()));
}

void LLVMContext::addFunc(const string &name, seq::BaseFunc *f, bool global) {
  add(name, make_shared<LLVMItem::Func>(f, getBase(), global || isToplevel()));
}

void LLVMContext::addImport(const string &name, const string &import,
                            bool global) {
  add(name,
      make_shared<LLVMItem::Import>(import, getBase(), global || isToplevel()));
}

void LLVMContext::addBlock(seq::Block *newBlock, seq::BaseFunc *newBase) {
  Context<LLVMItem::Item>::addBlock();
  if (newBlock)
    topBlockIndex = blocks.size();
  blocks.push_back(newBlock);
  if (newBase)
    topBaseIndex = bases.size();
  bases.push_back(newBase);
}

void LLVMContext::popBlock() {
  bases.pop_back();
  topBaseIndex = bases.size() - 1;
  while (topBaseIndex && !bases[topBaseIndex])
    topBaseIndex--;
  blocks.pop_back();
  topBlockIndex = blocks.size() - 1;
  while (topBlockIndex && !blocks[topBlockIndex])
    topBlockIndex--;
  Context<LLVMItem::Item>::popBlock();
}

void LLVMContext::initJIT() {
  jit = new seq::SeqJIT();
  auto fn = new seq::Func();
  fn->setName("$jit_0");

  addBlock(fn->getBlock(), fn);
  assert(topBaseIndex == topBlockIndex && topBlockIndex == 0);

  execJIT();
}

void LLVMContext::execJIT(string varName, seq::Expr *varExpr) {
  // static int counter = 0;

  // assert(jit != nullptr);
  // assert(bases.size() == 1);
  // jit->addFunc((seq::Func *)bases[0]);

  // vector<pair<string, shared_ptr<LLVMItem::Item>>> items;
  // for (auto &name : stack.front()) {
  //   auto i = find(name);
  //   if (i && i->isGlobal())
  //     items.push_back(make_pair(name, i));
  // }
  // popBlock();
  // for (auto &i : items)
  //   add(i.first, i.second);
  // if (varExpr) {
  //   auto var = jit->addVar(varExpr);
  //   add(varName, var);
  // }

  // // Set up new block
  // auto fn = new seq::Func();
  // fn->setName(format("$jit_{}", ++counter));
  // addBlock(fn->getBlock(), fn);
  // assert(topBaseIndex == topBlockIndex && topBlockIndex == 0);
}

// void LLVMContext::dump(int pad) {
//   auto ordered = decltype(map)(map.begin(), map.end());
//   for (auto &i : ordered) {
//     std::string s;
//     auto t = i.second.top();
//     if (auto im = t->getImport()) {
//       DBG("{}{:.<25} {}", string(pad*2, ' '), i.first, '<import>');
//       getImports()->getImport(im->getFile())->tctx->dump(pad+1);
//     }
//     else
//       DBG("{}{:.<25} {}", string(pad*2, ' '), i.first,
//       t->getType()->toString(true));
//   }
// }

shared_ptr<LLVMContext> LLVMContext::getContext(const string &file,
                                                shared_ptr<TypeContext> typeCtx,
                                                seq::SeqModule *module) {
  auto realizations = typeCtx->getRealizations();
  auto imports = typeCtx->getImports();
  auto stdlib = const_cast<ImportContext::Import *>(imports->getImport(""));

  auto block = module->getBlock();
  seq::BaseFunc *base = module;
  stdlib->lctx = make_shared<LLVMContext>(stdlib->filename, realizations,
                                          imports, block, base, nullptr);

  auto pod = vector<string>{"void",  "bool", "byte",    "int",
                            "float", "ptr",  "generic", "optional",
                            "Int",   "UInt", "tuple",   "function"};
  CodegenVisitor c(stdlib->lctx);
  // for (auto &p : pod)
  // c.visitMethods(p);
  c.transform(stdlib->statements.get());
  return make_shared<LLVMContext>(file, realizations, imports, block, base,
                                  nullptr);
}

} // namespace ast
} // namespace seq
