#pragma once

#include <deque>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/context.h"
#include "parser/common.h"

namespace seq {
namespace ast {

namespace LLVMItem {

class Import;
class Var;
class Func;
class Class;

class Item {
protected:
  seq::BaseFunc *base;
  bool global;
  std::unordered_set<std::string> attributes;

public:
  Item(seq::BaseFunc *base, bool global = false): base(base), global(global) {}
  virtual ~Item() {}

  const seq::BaseFunc *getBase() const { return base; }
  bool isGlobal() const { return global; }
  bool hasAttr(const std::string &s) const {
    return attributes.find(s) != attributes.end();
  }

  virtual const Class *getClass() const { return nullptr; }
  virtual const Func *getFunc() const { return nullptr; }
  virtual const Var *getVar() const { return nullptr; }
  virtual const Import *getImport() const { return nullptr; }
  virtual seq::Expr *getExpr() const = 0;
};

class Var : public Item {
  seq::Var *handle;

public:
  Var(seq::Var *var, seq::BaseFunc *base, bool global = false)
      : Item(base, global), handle(var) {}
  seq::Expr *getExpr() const override { return new seq::VarExpr(handle); }
  const Var *getVar() const override { return this; }
  seq::Var *getHandle() const { return handle; }
};

class Func : public Item {
  seq::BaseFunc *handle;

public:
  Func(seq::BaseFunc *f, seq::BaseFunc *base, bool global = false)
      : Item(base, global), handle(f) {}
  seq::Expr *getExpr() const override { return new seq::FuncExpr(handle); }
  const Func *getFunc() const override { return this; }
};

class Class : public Item {
  seq::types::Type *type;

public:
  Class(seq::types::Type *t, seq::BaseFunc *base, bool global = false)
      : Item(base, global), type(t) {}

  seq::types::Type *getType() const { return type; }
  seq::Expr *getExpr() const override { return new seq::TypeExpr(type); }
  const Class *getClass() const override { return this; }
};

class Import : public Item {
  std::string file;

public:
  Import(const std::string &file, seq::BaseFunc *base, bool global = false)
      : Item(base, global), file(file) {}
  seq::Expr *getExpr() const override { assert(false); }
  const Import *getImport() const override { return this; }
  std::string getFile() const { return file; }
};
} // namespace LLVMItem

class LLVMContext : public Context<LLVMItem::Item> {
  std::vector<seq::BaseFunc *> bases;
  std::vector<seq::Block *> blocks;
  int topBlockIndex, topBaseIndex;

  seq::TryCatch *tryCatch;
  seq::SeqJIT *jit;

public:
  LLVMContext(const std::string &filename,
              std::shared_ptr<RealizationContext> realizations,
              std::shared_ptr<ImportContext> imports, seq::Block *block,
              seq::BaseFunc *base, seq::SeqJIT *jit);
  virtual ~LLVMContext();

  std::shared_ptr<LLVMItem::Item> find(const std::string &name,
                                       bool onlyLocal = false,
                                       bool checkStdlib = true) const;

  using Context<LLVMItem::Item>::add;
  void addVar(const std::string &name, seq::Var *v, bool global = false);
  void addType(const std::string &name, seq::types::Type *t,
               bool global = false);
  void addFunc(const std::string &name, seq::BaseFunc *f, bool global = false);
  void addImport(const std::string &name, const std::string &import,
                 bool global = false);
  void addBlock(seq::Block *newBlock = nullptr,
                seq::BaseFunc *newBase = nullptr);
  void popBlock();

  void initJIT();
  void execJIT(std::string varName = "", seq::Expr *varExpr = nullptr);

public:
  seq::BaseFunc *getBase() const { return bases[topBaseIndex]; }
  seq::Block *getBlock() const { return blocks[topBlockIndex]; }
  seq::TryCatch *getTryCatch() const { return tryCatch; }
  void setTryCatch(seq::TryCatch *t) { tryCatch = t; }
  bool isToplevel() const { return bases.size() == 1; }
  seq::SeqJIT *getJIT() { return jit; }
  seq::types::Type *getType(const std::string &name) const {
    if (auto t = CAST(find(name), LLVMItem::Class))
      return t->getType();
    error("cannot find type '{}'", name);
    return nullptr;
  }
};

} // namespace ast
} // namespace seq
