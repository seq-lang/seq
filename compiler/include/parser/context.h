#pragma once

#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/common.h"
#include "seq/seq.h"

using std::shared_ptr;
using std::stack;
using std::string;
using std::unordered_map;
using std::unordered_set;

template <typename T> class VTable {
protected:
  unordered_map<string, stack<shared_ptr<T>>> map;
  stack<unordered_set<string>> stack;
  unordered_set<string> flags;

public:
  void add(const string &name, shared_ptr<T> var) {
    map[name].push(var);
    stack.top().insert(name);
  }
  void addBlock() { stack.push(unordered_set<string>()); }
  void popBlock() {
    for (auto &name : stack.top()) {
      auto i = map.find(name);
      if (i == map.end() || !i->second.size()) {
        error("variable {} not found in the map", name);
      }
      i->second.pop();
      if (!i->second.size()) {
        map.erase(name);
      }
    }
    stack.pop();
  }
  virtual shared_ptr<T> find(const string &name) const {
    auto it = map.find(name);
    if (it == map.end()) {
      return nullptr;
    }
    return it->second.top();
  }
  void setFlag(const string &s) {
    flags.insert(s);
  }
  void unsetFlag(const string &s) {
    flags.erase(s);
  }
  bool hasFlag(const string &s) {
    return flags.find(s) != flags.end();
  }
};

class ContextItem {
protected:
  seq::BaseFunc *base;
  bool toplevel;
  bool global;
  bool internal;
  unordered_set<string> attributes;

public:
  ContextItem(seq::BaseFunc *base, bool toplevel = false, bool global = false, bool internal = false);
  virtual ~ContextItem() {}
  virtual seq::Expr *getExpr() const = 0;

  const seq::BaseFunc *getBase() const;
  bool isGlobal() const;
  bool isToplevel() const;
  bool isInternal() const;
  bool hasAttr(const string &s) const;
};

class VarContextItem : public ContextItem {
  seq::Var *var;

public:
  VarContextItem(seq::Var *var, seq::BaseFunc *base, bool toplevel = false, bool global = false, bool internal = false);
  seq::Expr *getExpr() const override;
};

class FuncContextItem : public ContextItem {
  seq::Func *func;
  vector<string> names;

public:
  FuncContextItem(seq::Func *f, vector<string> n, seq::BaseFunc *base, bool toplevel = false, bool global = false, bool internal = false);
  seq::Expr *getExpr() const override;
};

class TypeContextItem : public ContextItem {
  seq::types::Type *type;

public:
  TypeContextItem(seq::types::Type *t, seq::BaseFunc *base, bool toplevel = false, bool global = false, bool internal = false);

  seq::types::Type *getType() const;
  seq::Expr *getExpr() const override;
};

class ImportContextItem : public ContextItem {
  string import;

public:
  seq::Expr *getExpr() const override;
};

class Context : public VTable<ContextItem> {
  string filename;
  seq::SeqModule *module;
  vector<seq::BaseFunc*> bases;
  vector<seq::Block*> blocks;
  int topBlockIndex, topBaseIndex;
  seq::types::Type *enclosingType;

  seq::TryCatch *tryCatch;
  unordered_map<string, VTable<ContextItem>> imports;
  VTable<ContextItem> stdlib;

public:
  Context(seq::SeqModule *module, const string &filename);
  shared_ptr<ContextItem> find(const string &name) const override;
  seq::TryCatch *getTryCatch() const;
  seq::Block *getBlock() const;
  seq::SeqModule *getModule() const;
  seq::BaseFunc *getBase() const;
  bool isToplevel() const;
  seq::types::Type *getType(const string &name) const;
  seq::types::Type *getEnclosingType();
  void setEnclosingType(seq::types::Type *t);
  void addBlock(seq::Block *newBlock, seq::BaseFunc *newBase);
  void popBlock();

  void add(const string &name, seq::Var *v);
  void add(const string &name, seq::types::Type *t);
  void add(const string &name, seq::Func *f, vector<string> names);
};
