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
using std::vector;

template <typename T> class VTable {
  typedef unordered_map<string, stack<shared_ptr<T>>> VTableMap;

protected:
  VTableMap map;
  stack<vector<string>> stack;
  unordered_set<string> flags;

  shared_ptr<T> find(const string &name) const {
    auto it = map.find(name);
    if (it == map.end()) {
      return nullptr;
    }
    return it->second.top();
  }

public:
  typename VTableMap::iterator begin() { return map.begin(); }
  typename VTableMap::iterator end() { return map.end(); }

  void add(const string &name, shared_ptr<T> var) {
    map[name].push(var);
    // DBG("adding {}", name);
    stack.top().push_back(name);
  }
  void addBlock() { stack.push(vector<string>()); }
  void popBlock() {
    for (auto &name : stack.top()) {
      // DBG("removing {}", name);
      remove(name);
    }
    stack.pop();
  }
  virtual void remove(const string &name) {
    auto i = map.find(name);
    if (i == map.end() || !i->second.size()) {
      error("variable {} not found in the map", name);
    }
    i->second.pop();
    if (!i->second.size()) {
      map.erase(name);
    }
  }
  void setFlag(const string &s) { flags.insert(s); }
  void unsetFlag(const string &s) { flags.erase(s); }
  bool hasFlag(const string &s) { return flags.find(s) != flags.end(); }
};

class ContextItem {
protected:
  seq::BaseFunc *base;
  bool global;
  unordered_set<string> attributes;

public:
  ContextItem(seq::BaseFunc *base, bool global = false);
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
  VarContextItem(seq::Var *var, seq::BaseFunc *base, bool global = false);
  seq::Expr *getExpr() const override;
  seq::Var *getVar() const;
};

class FuncContextItem : public ContextItem {
  seq::Func *func;
  vector<string> names;

public:
  FuncContextItem(seq::Func *f, vector<string> n, seq::BaseFunc *base,
                  bool global = false);
  seq::Expr *getExpr() const override;
};

class TypeContextItem : public ContextItem {
  seq::types::Type *type;

public:
  TypeContextItem(seq::types::Type *t, seq::BaseFunc *base,
                  bool global = false);

  seq::types::Type *getType() const;
  seq::Expr *getExpr() const override;
};

class ImportContextItem : public ContextItem {
  string import;

public:
  ImportContextItem(const string &import, seq::BaseFunc *base,
                    bool global = false);
  seq::Expr *getExpr() const override;
  string getFile() const;
};

class Context;
struct ImportCache {
  string argv0;
  Context *stdlib;
  unordered_map<string, shared_ptr<Context>> imports;

  string getImportFile(const string &what, const string &relativeTo,
                       bool forceStdlib = false);
  shared_ptr<Context> importFile(seq::SeqModule *module, const string &file);
};

class Context : public VTable<ContextItem> {
  ImportCache &cache;
  string filename;
  seq::SeqModule *module;
  vector<seq::BaseFunc *> bases;
  vector<seq::Block *> blocks;
  int topBlockIndex, topBaseIndex;
  seq::types::Type *enclosingType;

  seq::TryCatch *tryCatch;

public:
  Context(seq::SeqModule *module, ImportCache &cache,
          const string &filename = ""); // initialize standard library
  virtual ~Context() {}
  shared_ptr<ContextItem> find(const string &name,
                               bool onlyLocal = false) const;
  seq::TryCatch *getTryCatch() const;
  void setTryCatch(seq::TryCatch *t);
  seq::Block *getBlock() const;
  seq::SeqModule *getModule() const;
  seq::BaseFunc *getBase() const;
  bool isToplevel() const;
  seq::types::Type *getType(const string &name) const;
  seq::types::Type *getEnclosingType();
  void setEnclosingType(seq::types::Type *t);
  void addBlock(seq::Block *newBlock = nullptr,
                seq::BaseFunc *newBase = nullptr);
  void popBlock();

  void add(const string &name, shared_ptr<ContextItem> var);
  void add(const string &name, seq::Var *v, bool global = false);
  void add(const string &name, seq::types::Type *t, bool global = false);
  void add(const string &name, seq::Func *f, vector<string> names,
           bool global = false);
  void add(const string &name, const string &import, bool global = false);
  string getFilename() const;
  ImportCache &getCache();
};
