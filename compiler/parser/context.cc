#include <string>
#include <vector>

#include "parser/common.h"
#include "parser/context.h"
#include "seq/seq.h"

using fmt::format;


ContextItem::ContextItem(seq::BaseFunc *base, bool toplevel, bool global, bool internal):
  base(base), toplevel(toplevel), global(global), internal(internal) {}

const seq::BaseFunc *ContextItem::getBase() const {
  return base;
}
bool ContextItem::isGlobal() const { return global; }
bool ContextItem::isToplevel() const { return toplevel; };
bool ContextItem::isInternal() const { return internal; };
bool ContextItem::hasAttr(const string &s) const {
  return attributes.find(s) != attributes.end();
};

seq::Expr *VarContextItem::getExpr() const { return new seq::VarExpr(var); }
seq::Expr *FuncContextItem::getExpr() const { return new seq::FuncExpr(func); }
seq::Expr *TypeContextItem::getExpr() const { return new seq::TypeExpr(type); }
seq::types::Type *TypeContextItem::getType() const { return type; }
seq::Expr *ImportContextItem::getExpr() const {
  error("cannot use import item here");
  return nullptr;
}


Context::Context(seq::SeqModule *module, const string &filename):
  filename(filename), module(module), base(module), block(module->getBlock()), tryCatch(nullptr)
{
}

shared_ptr<ContextItem> Context::find(const string &name) const {
  auto i = VTable<ContextItem>::find(name);
  if (i && (i->isGlobal() || base == i->getBase())) {
    return i;
  } else {
    return nullptr;
  }
}

seq::BaseFunc *Context::getBase() const { return base; }
seq::types::Type *Context::getType(const string &name) const {
  if (auto i = find(name)) {
    if (auto t = dynamic_cast<TypeContextItem*>(i.get())) {
      return t->getType();
    }
  }
  error("cannot find type '{}'", name);
  return nullptr;
}
seq::Block *Context::getBlock() const { return block; }
seq::TryCatch *Context::getTryCatch() const {
  return tryCatch;
}