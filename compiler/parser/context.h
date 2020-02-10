#pragma once

#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "lang/seq.h"
#include "parser/common.h"

namespace seq {
namespace ast {

template <typename T> class VTable {
  typedef std::unordered_map<std::string, std::stack<std::shared_ptr<T>>>
      VTableMap;

protected:
  VTableMap map;
  std::stack<std::vector<std::string>> stack;
  std::unordered_set<std::string> flags;

  std::shared_ptr<T> find(const std::string &name) const {
    auto it = map.find(name);
    if (it == map.end()) {
      return nullptr;
    }
    return it->second.top();
  }

public:
  typename VTableMap::iterator begin() { return map.begin(); }
  typename VTableMap::iterator end() { return map.end(); }

  void add(const std::string &name, std::shared_ptr<T> var) {
    map[name].push(var);
    // DBG("adding {}", name);
    stack.top().push_back(name);
  }
  void addBlock() { stack.push(std::vector<std::string>()); }
  void popBlock() {
    for (auto &name : stack.top()) {
      // DBG("removing {}", name);
      remove(name);
    }
    stack.pop();
  }
  virtual void remove(const std::string &name) {
    auto i = map.find(name);
    if (i == map.end() || !i->second.size()) {
      error("variable {} not found in the map", name);
    }
    i->second.pop();
    if (!i->second.size()) {
      map.erase(name);
    }
  }
  void setFlag(const std::string &s) { flags.insert(s); }
  void unsetFlag(const std::string &s) { flags.erase(s); }
  bool hasFlag(const std::string &s) { return flags.find(s) != flags.end(); }
};

class ContextItem {
protected:
  seq::BaseFunc *base;
  bool global;
  std::unordered_set<std::string> attributes;

public:
  ContextItem(seq::BaseFunc *base, bool global = false);
  virtual ~ContextItem() {}
  virtual seq::Expr *getExpr() const = 0;

  const seq::BaseFunc *getBase() const;
  bool isGlobal() const;
  bool isToplevel() const;
  bool isInternal() const;
  bool hasAttr(const std::string &s) const;
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
  std::vector<std::string> names;

public:
  FuncContextItem(seq::Func *f, std::vector<std::string> n, seq::BaseFunc *base,
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
  std::string import;

public:
  ImportContextItem(const std::string &import, seq::BaseFunc *base,
                    bool global = false);
  seq::Expr *getExpr() const override;
  std::string getFile() const;
};

class Context;
struct ImportCache {
  std::string argv0;
  Context *stdlib;
  std::unordered_map<std::string, std::shared_ptr<Context>> imports;

  std::string getImportFile(const std::string &what,
                            const std::string &relativeTo,
                            bool forceStdlib = false);
  std::shared_ptr<Context> importFile(seq::SeqModule *module,
                                      const std::string &file);
};

class Context : public VTable<ContextItem> {
  ImportCache &cache;
  std::string filename;
  seq::SeqModule *module;
  std::vector<seq::BaseFunc *> bases;
  std::vector<seq::Block *> blocks;
  int topBlockIndex, topBaseIndex;
  seq::types::Type *enclosingType;

  seq::TryCatch *tryCatch;

public:
  Context(seq::SeqModule *module, ImportCache &cache,
          const std::string &filename = ""); // initialize standard library
  virtual ~Context() {}
  std::shared_ptr<ContextItem> find(const std::string &name,
                                    bool onlyLocal = false) const;
  seq::TryCatch *getTryCatch() const;
  void setTryCatch(seq::TryCatch *t);
  seq::Block *getBlock() const;
  seq::SeqModule *getModule() const;
  seq::BaseFunc *getBase() const;
  bool isToplevel() const;
  seq::types::Type *getType(const std::string &name) const;
  seq::types::Type *getEnclosingType();
  void setEnclosingType(seq::types::Type *t);
  void addBlock(seq::Block *newBlock = nullptr,
                seq::BaseFunc *newBase = nullptr);
  void popBlock();

  void add(const std::string &name, std::shared_ptr<ContextItem> var);
  void add(const std::string &name, seq::Var *v, bool global = false);
  void add(const std::string &name, seq::types::Type *t, bool global = false);
  void add(const std::string &name, seq::Func *f,
           std::vector<std::string> names, bool global = false);
  void add(const std::string &name, const std::string &import,
           bool global = false);
  std::string getFilename() const;
  ImportCache &getCache();
};

} // namespace ast
} // namespace seq
