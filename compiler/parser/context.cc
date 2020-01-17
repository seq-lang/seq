#include <string>
#include <vector>
#include <memory>

#include "parser/common.h"
#include "parser/context.h"
#include "seq/seq.h"

using fmt::format;
using std::pair;
using std::make_pair;
using std::make_shared;

const seq::BaseFunc *ContextItem::getBase() const {
  return base;
}
bool ContextItem::isGlobal() const { return global; }
bool ContextItem::isToplevel() const { return toplevel; };
bool ContextItem::isInternal() const { return internal; };
bool ContextItem::hasAttr(const string &s) const {
  return attributes.find(s) != attributes.end();
};

ContextItem::ContextItem(seq::BaseFunc *base, bool toplevel, bool global, bool internal):
base(base), toplevel(toplevel), global(global), internal(internal) {}
VarContextItem::VarContextItem(seq::Var *var, seq::BaseFunc *base, bool toplevel, bool global, bool internal):
  ContextItem(base, toplevel, global, internal), var(var) {}
seq::Expr *VarContextItem::getExpr() const { return new seq::VarExpr(var); }
seq::Var *VarContextItem::getVar() const { return var; }

FuncContextItem::FuncContextItem(seq::Func *func, vector<string> names, seq::BaseFunc *base, bool toplevel, bool global, bool internal):
  ContextItem(base, toplevel, global, internal), func(func), names(names) {}
seq::Expr *FuncContextItem::getExpr() const { return new seq::FuncExpr(func); }

TypeContextItem::TypeContextItem(seq::types::Type *type, seq::BaseFunc *base, bool toplevel, bool global, bool internal):
  ContextItem(base, toplevel, global, internal), type(type) {}
seq::Expr *TypeContextItem::getExpr() const { return new seq::TypeExpr(type); }
seq::types::Type *TypeContextItem::getType() const { return type; }
seq::Expr *ImportContextItem::getExpr() const {
  error("cannot use import item here");
  return nullptr;
}


Context::Context(seq::SeqModule *module, const string &filename):
  filename(filename), module(module), enclosingType(nullptr), tryCatch(nullptr), tmpVarCounter(0)
{
  module->setFileName(filename);
  stack.push(unordered_set<string>());
  bases.push_back(module);
  blocks.push_back(module->getBlock());
  topBaseIndex = topBlockIndex = 0;

  vector<pair<string, seq::types::Type*>> pods = {
    { "void", seq::types::Void },
    { "bool", seq::types::Bool },
    { "byte", seq::types::Byte },
    { "int", seq::types::Int },
    { "float", seq::types::Float },
    { "str", seq::types::Str },
    { "seq", seq::types::Seq }
  };
  for (auto &i: pods) {
    VTable<ContextItem>::add(i.first,
      make_shared<TypeContextItem>(i.second, getBase(), true, true, true));
  }
  add("__argv__", module->getArgVar());
}

shared_ptr<ContextItem> Context::find(const string &name) const {
  auto i = VTable<ContextItem>::find(name);
  if (i && (i->isGlobal() || getBase() == i->getBase())) {
    return i;
  } else {
    return nullptr;
  }
}

seq::BaseFunc *Context::getBase() const { return bases[topBaseIndex]; }
seq::SeqModule *Context::getModule() const { return module; }
seq::types::Type *Context::getType(const string &name) const {
  if (auto i = find(name)) {
    if (auto t = dynamic_cast<TypeContextItem*>(i.get())) {
      return t->getType();
    }
  }
  error("cannot find type '{}'", name);
  return nullptr;
}
seq::Block *Context::getBlock() const { return blocks[topBlockIndex]; }
seq::TryCatch *Context::getTryCatch() const {
  return tryCatch;
}

seq::types::Type *Context::getEnclosingType() {
  return enclosingType;
}

void Context::setEnclosingType(seq::types::Type *t) {
  enclosingType = t;
}

bool Context::isToplevel() const {
  return module == getBase();
}

void Context::addBlock(seq::Block *newBlock, seq::BaseFunc *newBase) {
  VTable<ContextItem>::addBlock();
  if (newBlock) {
    topBlockIndex = blocks.size();
  }
  blocks.push_back(newBlock);
  if (newBase) {
    topBaseIndex = bases.size();
  }
  bases.push_back(newBase);
  // fmt::print("[ctx] ++ block {} base {} mod {}\n", (void*)blocks[topBlockIndex], (void*)bases[topBaseIndex], (void*)module);
}

void Context::popBlock() {
  bases.pop_back();
  topBaseIndex = bases.size() - 1;
  while (!bases[topBaseIndex]) topBaseIndex--;
  blocks.pop_back();
  topBlockIndex = blocks.size() - 1;
  while (!blocks[topBlockIndex]) topBlockIndex--;
  VTable<ContextItem>::popBlock();
  // fmt::print("[ctx] -- block {} base {} mod {}\n", (void*)blocks[topBlockIndex], (void*)bases[topBaseIndex], (void*)module);
}

void Context::add(const string &name, seq::Var *v) {
  VTable<ContextItem>::add(name, make_shared<VarContextItem>(v, getBase(), isToplevel(), isToplevel()));
}

void Context::add(const string &name, seq::types::Type *t) {
  VTable<ContextItem>::add(name, make_shared<TypeContextItem>(t, getBase(), isToplevel(), isToplevel()));
}

void Context::add(const string &name, seq::Func *f, vector<string> names) {
  fmt::print("adding... {} {} \n", name, isToplevel());
  VTable<ContextItem>::add(name, make_shared<FuncContextItem>(f, names, getBase(), isToplevel(), isToplevel()));
}
