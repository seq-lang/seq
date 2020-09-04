#pragma once

#include <deque>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/cache.h"
#include "parser/ast/context.h"
#include "parser/common.h"

namespace seq {
namespace ast {

struct CodegenItem {
  enum Kind { Func, Type, Var } kind;
  seq::BaseFunc *base;
  bool global;
  std::unordered_set<std::string> attributes;
  union {
    seq::Var *var;
    seq::BaseFunc *func;
    seq::types::Type *type;
  } handle;

public:
  CodegenItem(Kind k, seq::BaseFunc *base, bool global = false)
      : kind(k), base(base), global(global) {}

  const seq::BaseFunc *getBase() const { return base; }
  bool isGlobal() const { return global; }
  bool isVar() const { return kind == Var; }
  bool isFunc() const { return kind == Func; }
  bool isType() const { return kind == Type; }
  seq::BaseFunc *getFunc() const { return isFunc() ? handle.func : nullptr; }
  seq::types::Type *getType() const { return isType() ? handle.type : nullptr; }
  seq::Var *getVar() const { return isVar() ? handle.var : nullptr; }
  bool hasAttr(const std::string &s) const {
    return attributes.find(s) != attributes.end();
  }
};

class CodegenContext : public Context<CodegenItem> {
  std::vector<seq::BaseFunc *> bases;
  std::vector<seq::Block *> blocks;
  int topBlockIndex, topBaseIndex;

public:
  std::shared_ptr<Cache> cache;
  seq::TryCatch *tryCatch;
  seq::SeqJIT *jit;
  std::unordered_map<std::string, seq::types::Type *> types;
  std::unordered_map<std::string, std::pair<seq::BaseFunc *, bool>> functions;

public:
  CodegenContext(std::shared_ptr<Cache> cache, seq::Block *block, seq::BaseFunc *base,
                 seq::SeqJIT *jit);

  std::shared_ptr<CodegenItem> find(const std::string &name, bool onlyLocal = false,
                                    bool checkStdlib = true) const;

  using Context<CodegenItem>::add;
  void addVar(const std::string &name, seq::Var *v, bool global = false);
  void addType(const std::string &name, seq::types::Type *t, bool global = false);
  void addFunc(const std::string &name, seq::BaseFunc *f, bool global = false);
  void addImport(const std::string &name, const std::string &import,
                 bool global = false);
  void addBlock(seq::Block *newBlock = nullptr, seq::BaseFunc *newBase = nullptr);
  void popBlock();

  void initJIT();
  void execJIT(std::string varName = "", seq::Expr *varExpr = nullptr);

  seq::types::Type *realizeType(types::ClassTypePtr t);

public:
  seq::BaseFunc *getBase() const { return bases[topBaseIndex]; }
  seq::Block *getBlock() const { return blocks[topBlockIndex]; }
  bool isToplevel() const { return bases.size() == 1; }
  seq::SeqJIT *getJIT() { return jit; }
  seq::types::Type *getType(const std::string &name) const {
    auto val = find(name);
    assert(val && val->getType());
    if (val)
      return val->getType();
    return nullptr;
  }
};

} // namespace ast
} // namespace seq
