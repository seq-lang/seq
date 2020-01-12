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

public:
  void add(const string &name, shared_ptr<T> var) {
    map[name].push(var);
    stack.top().insert(name);
  }
  void add_block() { stack.push(unordered_set<string>()); }
  void remove_block() {
    for (auto &name : stack.pop()) {
      auto i = map.find(name);
      if (i == map.end() || !i->second.size()) {
        throw InternalError("variable {} not found in the map", name);
      }
      i->second.pop();
      if (!i->second.size()) {
        map.erase(name);
      }
    }
  }
  virtual shared_ptr<T> find(const string &name) const {
    auto it = map.find(name);
    if (it == map.end()) {
      return nullptr;
    }
    return it->second.top();
  }
};

class ContextItem {
  seq::BaseFunc *base;
  bool toplevel;
  bool global;
  bool internal;
  unordered_set<string> attributes;

public:
  ContextItem(seq::BaseFunc *base, bool toplevel, bool global, bool internal);
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
  seq::Expr *getExpr() const override;
};

class FuncContextItem : public ContextItem {
  seq::Func *func;

public:
  seq::Expr *getExpr() const override;
};

class TypeContextItem : public ContextItem {
  seq::types::Type *type;

public:
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
  seq::BaseFunc *base;
  seq::Block *block;
  seq::TryCatch *tryCatch;
  unordered_set<string> flags;
  unordered_map<string, VTable<ContextItem>> imports;
  VTable<ContextItem> stdlib;

public:
  Context(seq::SeqModule *module, const string &filename);
  shared_ptr<ContextItem> find(const string &name) const override;
  seq::TryCatch *getTryCatch() const;
  seq::Block *getBlock() const;
  seq::BaseFunc *getBase() const;
  seq::types::Type *getType(const string &name) const;
};
