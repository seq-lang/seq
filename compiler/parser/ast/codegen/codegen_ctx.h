#pragma once

#include <deque>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/ast/cache.h"
#include "parser/ast/context.h"
#include "parser/common.h"

#include "sir/types/types.h"

#include "sir/sir.h"

namespace seq {
namespace ast {

struct CodegenItem {
  enum Kind { Func, Type, Var } kind;
  seq::ir::Func *base;
  bool global;
  unordered_set<string> attributes;
  union {
    seq::ir::Var *var;
    seq::ir::Func *func;
    seq::ir::types::Type *type;
  } handle;

public:
  CodegenItem(Kind k, seq::ir::Func *base, bool global = false)
      : kind(k), base(base), global(global) {}

  const seq::ir::Func *getBase() const { return base; }
  bool isGlobal() const { return global; }
  bool isVar() const { return kind == Var; }
  bool isFunc() const { return kind == Func; }
  bool isType() const { return kind == Type; }
  seq::ir::Func *getFunc() const { return isFunc() ? handle.func : nullptr; }
  seq::ir::types::Type *getType() const { return isType() ? handle.type : nullptr; }
  seq::ir::Var *getVar() const { return isVar() ? handle.var : nullptr; }
  bool hasAttr(const string &s) const { return attributes.find(s) != attributes.end(); }
};

class CodegenContext : public Context<CodegenItem> {
  vector<seq::ir::Func *> bases;
  vector<seq::ir::SeriesFlow *> series;
  int topBlockIndex, topBaseIndex;

public:
  shared_ptr<Cache> cache;
  // seq::SeqJIT *jit;
  unordered_map<string, seq::ir::types::Type *> types;
  unordered_map<string, pair<seq::ir::Func *, bool>> functions;

public:
  CodegenContext(shared_ptr<Cache> cache, seq::ir::SeriesFlow *top, seq::ir::Func *base
                 // ,seq::SeqJIT *jit
  );

  shared_ptr<CodegenItem> find(const string &name, bool onlyLocal = false,
                               bool checkStdlib = true) const;

  using Context<CodegenItem>::add;
  void addVar(const string &name, seq::ir::Var *v, bool global = false);
  void addType(const string &name, seq::ir::types::Type *t, bool global = false);
  void addFunc(const string &name, seq::ir::Func *f, bool global = false);
  void addImport(const string &name, const string &import, bool global = false);
  void addSeries(seq::ir::SeriesFlow *s = nullptr, seq::ir::Func *newBase = nullptr);
  void popSeries();

  void addScope() { Context<CodegenItem>::addBlock(); }
  void popScope() { Context<CodegenItem>::popBlock(); }

  //  void initJIT();
  //  void execJIT(string varName = "", seq::Expr *varExpr = nullptr);

  seq::ir::types::Type *realizeType(types::ClassTypePtr t);

public:
  seq::ir::Func *getBase() const { return bases[topBaseIndex]; }
  seq::ir::SeriesFlow *getSeries() const { return series[topBlockIndex]; }
  seq::ir::BlockFlow *getInsertPoint() const {
    return dynamic_cast<seq::ir::BlockFlow *>(
        series[topBlockIndex]->series.back().get());
  }
  seq::ir::SIRModule *getModule() const {
    return dynamic_cast<seq::ir::SIRModule *>(bases[0]->parent);
  }
  bool isToplevel() const { return bases.size() == 1; }
  //  seq::SeqJIT *getJIT() { return jit; }
  seq::ir::types::Type *getType(const string &name) const {
    auto val = find(name);
    assert(val && val->getType());
    if (val)
      return val->getType();
    return nullptr;
  }
  seq::ir::types::ArrayType *getArgvType();

private:
  seq::ir::types::PointerType *getPointer(types::ClassTypePtr t);
};

} // namespace ast
} // namespace seq
